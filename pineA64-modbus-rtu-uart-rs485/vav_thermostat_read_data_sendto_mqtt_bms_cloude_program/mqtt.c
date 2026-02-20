#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <MQTTAsync.h>
#include <strings.h>
#include <errno.h>
#include <time.h>

#include "mqtt.h"
#include "config.h"
#include "data.h"

#define QOS 0
#define BUF_SIZE 512
#define MAX_PENDING_ACKS 32

static MQTTAsync mqtt_client = NULL;

int mqtt_is_connected = 0;

// commands related structure for modbus write to particular register
CommandContext cmd_ctx = {0};

#define DATA_STORE_FILE "mqtt_store_data.txt"
#define TEMP_STORE_FILE "tmp_store.txt"


// Global publish interval (in seconds)
time_t publish_interval_sec = 60; // default

// file where we store last polling interval
static const char *INTERVAL_FILE = "/home/zedbee/vav/vav_thermo/publishInterval.txt";

static int store_on_publish_fail = 0;
static time_t last_offline_store_time = 0;

// ACK structure, Bms commands send json that one seprate take to write modbus write
typedef struct 
{
  char corr_id[128];
  int success;
  char reason[128];
  char equipment_id[64];
  char command[32];
} PendingAck;

//  PendingAck structure create one buffer ack_queue
static PendingAck ack_queue[MAX_PENDING_ACKS];
static int ack_head = 0;
static int ack_tail = 0;

//  pthread create 
static pthread_mutex_t ack_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t ack_sem;

static volatile int program_running = 1;

// Forward declarations
static void connlost(void *ctx, char *cause);
static void onConnect(void *ctx, MQTTAsync_successData *response);
static void onConnectFailure(void *ctx, MQTTAsync_failureData *response);
static void onSubscribeFailure(void *context, MQTTAsync_failureData *response);
static int messageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message);

//  acknowledge sender thread
static void *ack_sender_thread(void *arg);

//  this function to for bms command take and split 
static void queue_ack(const char *corr_id, int success, const char *reason, const char *equipment_id, const char *command);

//  update store polling interval
static void update_store_policy_based_on_interval(void) 
{
  if (publish_interval_sec > 60) 
  {
    store_on_publish_fail = 1;
    printf("[POLICY] Long interval (%ld s) ---> STORE failed messages + forward on reconnect\n", publish_interval_sec);
  }
  else
  {
    store_on_publish_fail = 0;
    printf("[POLICY] Short/normal interval (%ld s) ---> DROP failed messages\n", publish_interval_sec);
  }
}

void load_publish_interval(void)
{
  //  publishInterval.txt this file open to read polling interval time

  //  FILE *fopen(const char *pathname, const char *mode);
  FILE *fp = fopen(INTERVAL_FILE, "r");
  if (fp) 
  {
    long value;

    //  int fscanf(FILE *stream, const char *format, ...);
    if (fscanf(fp, "%ld", &value) == 1 && value >= 10 && value <= 3600) 
    {
      //  polling innterval time value passed to publish interval seconds 
      publish_interval_sec = value;
      printf("[STARTUP] Loaded publish interval: %ld seconds\n", publish_interval_sec);
    } 
    else 
    {
      printf("[STARTUP] Invalid value in publishInterval.txt → default %ld s\n", publish_interval_sec);
    }

    //  int fclose(FILE *stream); 
    fclose(fp);
  } 
  else
  {
    printf("[STARTUP] No publishInterval.txt → using default %ld s\n", publish_interval_sec);
  }

  //  this function take for polling interval time take
  update_store_policy_based_on_interval();
}

//  bms send polling interval time save to this file publishInterval.txt  
static void save_publish_interval(time_t new_interval)
{
  //  FILE *fopen(const char *pathname, const char *mode);
  FILE *fp = fopen(INTERVAL_FILE, "w");
  if (fp)
  {
    //  int fprintf(FILE *stream, const char *format, ...);
    fprintf(fp, "%ld", new_interval);

    //  int fclose(FILE *stream);
    fclose(fp);
    printf("[POLLING] Saved new interval: %ld seconds\n", new_interval);
  }
  else
  {
    perror("[POLLING] Failed to save interval");
  }
}

//  mqtt failed or network failed mqtt publish  data store to this file  =======> mqtt_store_data.txt
void store_message(const char *topic, const char *payload)
{
  printf("\n\nAttempting to store message\n\n");

  //  mqtt_store_data.txt  create file and open write only file permission given for rw-r-r
  //  int open(const char *pathname, int flags, mode_t mode);
  int fd = open(DATA_STORE_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
  if (fd == -1)
  {
    //  int fprintf(FILE *stream, const char *format, ...);
    fprintf(stderr, "store message open failed : %s\n", strerror(errno));

    //  int fclose(FILE *stream);
    close(fd);

    return;
  }

  //  FILE *fdopen(int fd, const char *mode);
  FILE *fp = fdopen(fd, "a");
  if (!fp)
  {
    fprintf(stderr, "store_message fdopen failed : %s\n", strerror(errno));
    close(fd);
    return;
  }

  //  current time take

  //  time_t time(time_t *tloc);
  time_t now = time(NULL);

  //  mqtt data payload take 
  if (fprintf(fp, "%ld|%s|%s\n", (long)now, topic, payload) < 0)
  {
    fprintf(stderr, "store_message fprintf failed : %s\n", strerror(errno));
  }

  if (fclose(fp) != 0)
  {
    fprintf(stderr, "store message fclose failed : %s\n", strerror(errno));
  }
  else
  {
    printf("Stored locally topic '%s' : %s\n", topic, payload);
  }
}

//  when network connect and mqtt connect that time store mqtt data forward with timestamp 
void forward_stored_messages(MQTTAsync *client)
{
  printf("\n\nAttempting to forward stored message\n\n");

  //  open the file for ---> mqtt_store_data.txt and the read permission give file ---> -rw-r--r-- 1 root root 0 Feb 16 15:36 mqtt_store_data.txt

  //  int open(const char *pathname, int flags);
  int fd = open(DATA_STORE_FILE, O_RDONLY);
  if (fd == -1) 
  {
    printf("\n********************************** No Stored message to forward (file does not exist) ******************\n");
    return;
  }

  //  FILE *fdopen(int fd, const char *mode);
  FILE *fp = fdopen(fd, "r");
  if (!fp)
  {
    fprintf(stderr,"********** forward_stored_messages fdopen failed : %s\n", strerror(errno));
    close(fd);
  }

  //  create and open the file for ---> tmp_store.txt
  //  int open(const char *pathname, int flags, mode_t mode);
  int temp_fd = open(TEMP_STORE_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (temp_fd == -1)
  {
    fprintf(stderr, "forward_stored_message temp open failed: %s\n", strerror(errno));
    fclose(fp);
    return;
  }
  else
  {
    printf("open the temp file success *****************\n\n");
  }

  //  FILE *fdopen(int fd, const char *mode);
  FILE *temp_fp = fdopen(temp_fd, "w");
  if (!temp_fp)
  {
    fprintf(stderr, "forward_stored_messages temp fdopen failed : %s\n", strerror(errno));
    fclose(fp);
    close(temp_fd);
    return;
  }

  //  character bufer create.  mqtt data payload
  char line[2048];

  //  char *fgets(char *s, int size, FILE *stream);
  while (fgets(line, sizeof(line), fp))
  {
    char *saveptr;
    char *ts = strtok_r(line, "|", &saveptr);
    char *topic = strtok_r(NULL, "|", &saveptr);
    char *payload = strtok_r(NULL, "\n", &saveptr);

    if (!ts || !topic || !payload) 
    {
      fprintf(temp_fp, "%s", line);
      continue;
    }

    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = payload;
    msg.payloadlen = strlen(payload);
    msg.qos = QOS;
    msg.retained = 0;

    //  mqtt data MQTTAsync_sendMessage to forward to mqtt data
    int rc = MQTTAsync_sendMessage(*client, topic, &msg, NULL);
    if (rc != MQTTASYNC_SUCCESS) 
    {
      fprintf(temp_fp, "%s|%s|%s\n", ts, topic, payload);
      printf("Failed to forward stored message to topic '%s': %s\n\n", topic, payload);
    }
    else 
    {
      printf("***********Forward stored messaged to topic********** '%s' : %s\n\n", topic, payload);
    }
  }
  fclose(fp);
  fclose(temp_fp);

  //  rename for mqtt_store_data.txt file to tmp_store.txt. this one use means store mqtt data and forward mqtt data after the 
  //  int rename(const char *oldpath, const char *newpath)
  if (rename(TEMP_STORE_FILE, DATA_STORE_FILE) != 0) 
  {
    fprintf(stderr, "forward_stored_messages rename failed : %s\n", strerror(errno));
  }
}


// ACK thread and queue functions (unchanged)

static void *ack_sender_thread(void *arg) 
{
  // MQTT Asynchronous 
  MQTTAsync *client = (MQTTAsync *)arg;

  while (program_running)
  {
    //  int sem_wait(sem_t *sem);
    sem_wait(&ack_sem);
    if (!program_running) 
    {
      break;
    }

    //  pthread mutex lock to for ack_mutex address
    pthread_mutex_lock(&ack_mutex);
    if (ack_head == ack_tail) 
    {
      //  pthread mutex unlock to for ack_mutex address
      pthread_mutex_unlock(&ack_mutex);
      continue;
    }

    //  PendigAck for structure
    PendingAck item = ack_queue[ack_head];

    ack_head = (ack_head + 1) % MAX_PENDING_ACKS;

    pthread_mutex_unlock(&ack_mutex);

    if (!mqtt_is_connected || !client)
    {
      continue;
    }

    char ack[1024];

    //  this for bms commands send means this like comming
    snprintf(ack, sizeof(ack),
	"{\"corr_id\":\"%s\",\"ack\":%d,\"reason\":\"%s\",\"equipment_id\":\"%s\",\"command\":\"%s\"}",
	item.corr_id, item.success, item.reason, item.equipment_id, item.command);

    //  Mqtt async
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = ack;
    msg.payloadlen = (int)strlen(ack);
    msg.qos = QOS;
    msg.retained = 0;

    //  MqttAsync send messasge
    int rc = MQTTAsync_sendMessage(*client, BOODSKAP_ACK_TOPIC, &msg, NULL);
    if (rc == MQTTASYNC_SUCCESS)
    {
      printf("[ACK sent] %s\n", ack);
    }
    else 
    {
      printf("[ACK failed rc=%d] %s\n", rc, ack);
    }
  }
  return NULL;
}

//  this for mqtt queue acknowledge.  bms commands send json that seprate take like corr_id, success, reason, equipment id, command
static void queue_ack(const char *corr_id, int success, const char *reason, const char *equipment_id, const char *command)
{
  pthread_mutex_lock(&ack_mutex);
  int next = (ack_tail + 1) % MAX_PENDING_ACKS;
  if (next == ack_head)
  {
    printf("[ACK] Queue full – dropping\n");
    pthread_mutex_unlock(&ack_mutex);
    return;
  }

  PendingAck *item = &ack_queue[ack_tail];

  strncpy(item->corr_id, corr_id ? corr_id : "unknown", sizeof(item->corr_id)-1);
  strncpy(item->reason, reason ? reason : "", sizeof(item->reason)-1);
  strncpy(item->equipment_id, equipment_id ? equipment_id : "", sizeof(item->equipment_id)-1);
  strncpy(item->command, command ? command : "", sizeof(item->command)-1);

  item->corr_id[sizeof(item->corr_id)-1] = '\0';
  item->reason[sizeof(item->reason)-1] = '\0';
  item->equipment_id[sizeof(item->equipment_id)-1] = '\0';
  item->command[sizeof(item->command)-1] = '\0';
  item->success = success;

  ack_tail = next;

  sem_post(&ack_sem);

  pthread_mutex_unlock(&ack_mutex);

  printf("[ACK queued] corr_id=%s success=%d cmd=%s reason=\"%s\"\n",
      item->corr_id, success, item->command, item->reason);
}

// MQTT callbacks

//  mqtt connection lost
static void connlost(void *ctx, char *cause) 
{
  (void)ctx;
  printf("[MQTT] Connection lost: %s\n", cause ? cause : "unknown");
  mqtt_is_connected = 0;
}

//  mqtt on connect
static void onConnect(void *ctx, MQTTAsync_successData *response) 
{
  MQTTAsync *client = (MQTTAsync *)ctx;
  printf("[MQTT] Connected (session present: %d)\n", response ? response->alt.connect.sessionPresent : 0);
  mqtt_is_connected = 1;

  // when mqtt connect and network connect forward messsage mqtt data
  forward_stored_messages(client);

  //  subscribe
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  opts.onFailure = onSubscribeFailure;
  opts.context = client;

  //  Mqtt Asynchronus subscribe topic
  int rc = MQTTAsync_subscribe(*client, BOODSKAP_SUBSCRIBE_TOPIC, QOS, &opts);
  if (rc == MQTTASYNC_SUCCESS)
  {
    printf("[MQTT] Subscribe sent → %s\n", BOODSKAP_SUBSCRIBE_TOPIC);
  }
  else
  {
    printf("[MQTT] Subscribe failed: %d\n", rc);
  }
}

//  mqtt connected failure
static void onConnectFailure(void *ctx, MQTTAsync_failureData *response)
{
  (void)ctx;
  printf("[MQTT] Connect failed: %d\n", response ? response->code : -1);
  mqtt_is_connected = 0;
}

//  mqtt subcribe failure
static void onSubscribeFailure(void *context, MQTTAsync_failureData *response) 
{
  (void)context;
  printf("[MQTT] Subscribe failed code=%d msg=%s\n",
      response ? response->code : -1,
      response && response->message ? response->message : "unknown");
}

//  this one for main. message arrived  for bms send commands. here take seprarte. topic name, topic len
static int messageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message) 
{
  (void)context;
  (void)topicLen;

  char payload[BUF_SIZE] = {0};
  int len = message->payloadlen < BUF_SIZE ? message->payloadlen : BUF_SIZE - 1;

  //  void *memcpy(void *dest, const void *src, size_t n);
  memcpy(payload, message->payload, len);
  payload[len] = '\0';

  // BMS commands sent for formate print what give also ahowing
  printf("BMS cmds MQTT Received on topic %s: %s\n", topicName, payload);

  char corr_id[128]     = "unknown";
  char equipment_id[64] = "";
  char command[32]      = "";
  char param[64]        = "";
  char reason[128]      = "unsupported command";   // MOVED UP – fixes undeclared error
  int success           = 0;

  // try Json format first old style
  const char *json_start = strchr(payload, '{');
  if (json_start)
  {
    //  Json - parse corr_id and equipment_id

    //  Parse corr_id

    //  char *strstr(const char *haystack, const char *needle);
    const char *pos = strstr(payload, "\"corr_id\":\"");

    if (pos)
    {
      pos += 11;

      //  int sscanf(const char *str, const char *format, ...);
      sscanf(pos, "%127[^\"]", corr_id);
    }

    // Parse equipment_id
    pos = strstr(payload, "\"equipment_id\":\"");
    if (pos) 
    {
      pos += 16;
      sscanf(pos, "%63[^\"]", equipment_id);
    }

    printf("[DEBUG] After parsing ids → corr_id='%s' equip='%s'\n", corr_id, equipment_id);


    // Command detection – robust for quoted & unquoted
    struct { const char *key; const char *name; int reg; } cmd_map[] = {
      {"\"vas\":", "vas", 1},
      {"\"mod\":", "mod", 3},
      {"\"st2\":", "st2", 11},
      {"\"sps\":", "sps", 16},
      {NULL, NULL, 0}
    };

    int found = 0;
    for (int i = 0; cmd_map[i].key && !found; i++) 
    {
      const char *key_pos = strstr(payload, cmd_map[i].key);
      if (key_pos) 
      {
	const char *value_start = key_pos + strlen(cmd_map[i].key);

	// Skip whitespace after colon
	while (isspace((unsigned char)*value_start))
	{
	  value_start++;
	}

	printf("[DEBUG] Found key '%s' at pos, next char='%c'\n", cmd_map[i].key, *value_start ? *value_start : '?');

	char temp[64] = {0};

	if (*value_start == '"')
	{
	  // Quoted string
	  value_start++;

	  // corr_id split and take
	  if (sscanf(value_start, "%63[^\"]", temp) == 1)
	  {
	    strcpy(param, temp);
	    found = 1;
	    printf("[DEBUG] Quoted value: '%s'\n", param);
	  }
	}
	else if (isdigit((unsigned char)*value_start) || *value_start == '-' || *value_start == '.')
	{
	  // Unquoted number
	  if (sscanf(value_start, "%63[^,}]", temp) == 1)
	  {
	    // Trim trailing whitespace
	    char *end = temp + strlen(temp) - 1;
	    while (end >= temp && isspace((unsigned char)*end)) *end-- = '\0';
	    strcpy(param, temp);
	    found = 1;
	    printf("[DEBUG] Unquoted value: '%s'\n", param);
	  }
	}

	if (found)
	{
	  strcpy(command, cmd_map[i].name);
	}

	if (!found) 
	{
	  printf("[CMD] No supported command found in payload\n");
	}
	printf("[CMD] Parsed → cmd='%s' param='%s'\n", command, param);
      }
    }
  }
  else
  {
    /* bms commands send polling interval and get status. this json plain comming */

    // Comma-separated plain text format (new style)
    // Format: corr_id,equipment_id,command,param
    char *tokens[4] = {0};
    char *token = strtok(payload, ",");
    int idx = 0;
    while (token && idx < 4) 
    {
      tokens[idx++] = token;
      token = strtok(NULL, ",");
    }

    if (idx >= 3) 
    {
      strncpy(corr_id,      tokens[0], sizeof(corr_id)-1);
      strncpy(equipment_id, tokens[1], sizeof(equipment_id)-1);
      strncpy(command,      tokens[2], sizeof(command)-1);

      if (idx == 4) 
      {
	strncpy(param, tokens[3], sizeof(param)-1);
      }

      // Trim trailing \n or \r if any
      char *end = command + strlen(command) - 1;
      while (end >= command && (*end == '\n' || *end == '\r')) *end-- = '\0';

      printf("[DEBUG] Parsed plain text: corr='%s' equip='%s' cmd='%s' param='%s'\n", corr_id, equipment_id, command, param);
    }
  }

  for (char *c = command; *c; c++) *c = tolower((unsigned char)*c);

  printf("[CMD] corr_id='%s' equip='%s' cmd='%s' param='%s'\n", corr_id, equipment_id, command, param);

  // Special command: "get" → immediate publish of current status

  if (strcmp(command, "get") == 0)
  {
    success = 1;

    // Immediately read current Modbus registers
    uint16_t regs[NUM_REGS] = {0};
    int n = modbus_read_registers(cmd_ctx.modbus_ctx, 0, NUM_REGS, regs);
    if (n != NUM_REGS) 
    {
      printf("modbus_read_registers failed for get command\n");
      snprintf(reason, sizeof(reason), "failed to read current status");
    }
    else
    {
      // Fill sensor struct (copy from your main loop logic). this one when bms give get status immediate publish data send to mqtt data publish

      //  vav sensor data sturcture access this each register access
      VavSensorData sensor = {0};
      sensor.device_id          = regs[0];
      sensor.vav_status         = regs[1];
      sensor.v2s_source         = regs[2];
      sensor.mode               = regs[3];
      sensor.cfm                = regs[4];
      sensor.ccf                = regs[5];
      sensor.vnm                = regs[6];
      sensor.vmn                = regs[7];
      sensor.vmx                = regs[8];
      sensor.dmx                = regs[9];
      sensor.dmp                = regs[10];
      sensor.set_temp           = regs[11];
      sensor.amb_temp           = regs[12];
      sensor.pir_status         = regs[13];
      sensor.amb_temp_offset    = regs[14];
      sensor.pfc_control        = regs[15];
      sensor.direction          = regs[16];
      sensor.set_damper_position = regs[17];

      // Print current poll for debug
      printf("\n--- Immediate Poll for GET ---\n");
      for (int i = 0; i < NUM_REGS; i++) 
      {
	// this function print output all register data in terminal output 
	print_register(i, regs[i]);   // assuming you have this function
      }

      // Create and publish JSON immediately
      char buf[BUFFER_SIZE1];

      // current time take 
      time_t now = time(NULL);

      //  data send to json format
      createVavJson(buf, &sensor, 1, now);   // your JSON function
      printf("\nImmediate Publish for GET:\n%s\n", buf);

      //  this immediate mqtt data publish 
      if (MQTT_Publish(&mqtt_client, BOODSKAP_PUBLISH_TOPIC, buf) == 0) 
      {
	printf("[GET] Immediate publish success\n");
	reason[0] = '\0';
      }
      else 
      {
	snprintf(reason, sizeof(reason), "publish failed");
      }
    }
  }

  // Special handling: pollinginterval command
  if (strcasecmp(command, "pollinginterval") == 0 && param[0] != '\0')
  {
    success = 1;
    char unit[8] = {0};
    int value = 0;

    //  input format conversion

    //  int sscanf(const char *str, const char *format, ...);
    sscanf(param, "%d%s", &value, unit);

    time_t new_interval = 0;

    //  int strcasecmp(const char *s1, const char *s2);
    //  this for ---> 5m <--- this like come for bms commands 
    if (strcasecmp(unit, "m") == 0 || strcasecmp(unit, "min") == 0) 
    {
      new_interval = value * 60;     // this minutes taken example 5 minutes.  5 * 60 = 300 sec
    }
    else if (strcasecmp(unit, "s") == 0 || strcasecmp(unit, "sec") == 0) 
    {
      new_interval = value;         // this for seconds taken example 5 seconds
    }
    else if (strcasecmp(unit, "h") == 0 || strcasecmp(unit, "hr") == 0)
    {
      new_interval = value * 3600;  // this for hour  taken example 1 hour
    }

    if (new_interval >= 10 && new_interval <= 3600)
    { 
      // open the files for polling interval time store value for 

      // reasonable range: 10s ~ 1h
      publish_interval_sec = new_interval;

      // === Important: Enable forced store-on-fail mode ===
      // force_store_on_fail = 1;

      // mqtt failed update time 120
      update_store_policy_based_on_interval();

      // bms send time  2 minutes means 120 save to this file
      save_publish_interval(new_interval);  // save to file

      printf("[POLLING] Publish interval changed to %ld seconds (%d %s)\n", publish_interval_sec, value, unit);
      snprintf(reason, sizeof(reason), "");
      reason[0] = '\0';
    }
    else
    {
      snprintf(reason, sizeof(reason), "invalid pollinginterval value");
    }
  }

  //  Command handling for vav thermostat register and mqtt read and write modbus register vav parameter
  if (strcmp(command, "vas") == 0)  // comparing.  "vas" ---> this for bms mnemonics.
  {
    //  int atoi(const char *nptr);  
    int value = atoi(param);  // convert a string to an integer
    printf("VAS command ---> value = %d\n", value); 
    success = 1;

    if (value == 0 || value == 1) 
    {
      //  this one modbus write to vav thermostat device register. like "vas" register to write
      //  int modbus_write_register(modbus_t *ctx, int addr, const uint16_t value);
      int rc = modbus_write_register(cmd_ctx.modbus_ctx, 1, (uint16_t)value);
      if (rc == -1) 
      {
	printf("modbus vas write failed\n");
	snprintf(reason, sizeof(reason), "modbus write failed");
      } 
      else
      {
	printf("Modbus VAS write %s success\n", value ? "ON" : "OFF");
	reason[0] = '\0';
      }
    } 
    else 
    {
      snprintf(reason, sizeof(reason), "invalid vas value (0 or 1 only)");
    }
  }
  else if (strcmp(command, "mod") == 0)  // comparing. "mod" ---> this for bms mnemonics.
  {
    //  int atoi(const char *nptr);
    int value = atoi(param);  // convert a string to an integer
    printf("MOD command → value = %d\n", value);
    success = 1;

    if (value == 0 || value == 1) 
    {
      //  this one modbus write to vav thermostat device register. like "mod" register to write
      //  int modbus_write_register(modbus_t *ctx, int addr, const uint16_t value);
      int rc = modbus_write_register(cmd_ctx.modbus_ctx, 3, (uint16_t)value);
      if (rc == -1)
      {
	printf("modbus mod write failed\n");
	snprintf(reason, sizeof(reason), "modbus write failed");
      } 
      else
      {
	printf("Modbus MOD write success\n");
	reason[0] = '\0';
      }
    } 
    else
    {
      snprintf(reason, sizeof(reason), "invalid mod value (0 or 1 only)");
    }
  }
  else if (strcmp(command, "st2") == 0)  // comparing. "st2" ---> this for bms mnemonics.
  {
    success = 1;
    float temp_c = atof(param);

    printf("ST2 command ---> temp = %.2f\n", temp_c);

    uint16_t reg_value = (uint16_t)(temp_c * 100.0f + 0.5f);


    //  this one modbus write to vav thermostat device register. like "st2" register to write
    //  int modbus_write_register(modbus_t *ctx, int addr, const uint16_t value);
    int rc = modbus_write_register(cmd_ctx.modbus_ctx, 11, reg_value);
    if (rc == -1)
    {
      printf("modbus st2 write failed\n");
      snprintf(reason, sizeof(reason), "modbus write failed");
    }
    else 
    {
      printf("Modbus ST2 write success (%.2f °C)\n", temp_c);
      reason[0] = '\0';
    }
  }
  else if (strcmp(command, "sps") == 0)  // comparing "st2" ---> this for bms mnemonics.
  {
    success = 1;
    float percent = atof(param);
    printf("SPS command ---> percent = %.1f\n", percent);

    if (percent >= 0 && percent <= 100) 
    {
      uint16_t reg_value = (uint16_t)(percent);  // 10.0f + 0.5f);

      //  this one modbus write to vav thermostat device register. like "sps" register to write
      //  int modbus_write_register(modbus_t *ctx, int addr, const uint16_t value);
      int rc = modbus_write_register(cmd_ctx.modbus_ctx, 17, reg_value);
      if (rc == -1) 
      {
	printf("modbus sps write failed\n");
	snprintf(reason, sizeof(reason), "modbus write failed");
      }
      else 
      {
	printf("Modbus SPS write success (%.1f %%)\n", percent);
	reason[0] = '\0';
      }
    } 
    else
    {
      snprintf(reason, sizeof(reason), "sps value must be 0–100");
    }
  }

  //  this one send bms acknowledge for corr_id, success, reason, equipment, command
  queue_ack(corr_id, success, reason, equipment_id, command);

  MQTTAsync_freeMessage(&message);
  MQTTAsync_free(topicName);
  return 1;
}

//  Public API
//  Mqtt initializing 
void MQTT_Init(MQTTAsync *client) 
{
  int rc = MQTTAsync_create(client, BOODSKAP_BROKER, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

  if (rc != MQTTASYNC_SUCCESS)
  {
    fprintf(stderr, "[MQTT] create failed: %d\n", rc);
    exit(1);
  }

  mqtt_client = *client;

  //  mqtt set call backs
  MQTTAsync_setCallbacks(*client, NULL, connlost, messageArrived, NULL);
}

//  mqtt connect related setup
int8_t MQTT_Connect(MQTTAsync *client, const char *user, const char *pass) 
{
  MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
  opts.keepAliveInterval = 20;
  opts.cleansession = 1;
  opts.onSuccess = onConnect;
  opts.onFailure = onConnectFailure;
  opts.context = client;
  opts.username = user;
  opts.password = pass;

  int rc = MQTTAsync_connect(*client, &opts);
  if (rc != MQTTASYNC_SUCCESS) 
  {
    printf("Mqtt connect call failed: %d\n", rc);
    return -1;
  }

  // ptherad create
  sem_init(&ack_sem, 0, 0);
  pthread_t th;
  pthread_create(&th, NULL, ack_sender_thread, client);
  pthread_detach(th);

  return 0;
}

//  mqtt disconnect related setup
void MQTT_Disconnect(MQTTAsync *client) 
{
  program_running = 0;
  sem_post(&ack_sem);
  MQTTAsync_disconnect(*client, NULL);
}

//  mqtt publish related setup
int8_t MQTT_Publish(MQTTAsync *client, const char *topic, const char *payload) 
{
  time_t now = time(NULL);

  if (!mqtt_is_connected || !client)
  {
    if (now - last_offline_store_time < publish_interval_sec)
    {
      return -1; // throttle
    }
    last_offline_store_time = now;

    if (store_on_publish_fail)
    {
      printf("[OFFLINE STORE] interval=%ld s → %s\n", publish_interval_sec, topic);
      store_message(topic, payload);
    }
    return -1;
  }

  MQTTAsync_message msg = MQTTAsync_message_initializer;
  msg.payload = (void *)payload;
  msg.payloadlen = (int)strlen(payload);
  msg.qos = QOS;
  msg.retained = 0;

  int rc = MQTTAsync_sendMessage(*client, topic, &msg, NULL);

  if (rc != MQTTASYNC_SUCCESS) {
    printf("[PUBLISH FAIL rc=%d] ", rc);

    if (now - last_offline_store_time >= publish_interval_sec)
    {
      last_offline_store_time = now;
      if (store_on_publish_fail) 
      {
	// Mqtt failed store the topic and mqtt data payload this function
	store_message(topic, payload);

	printf("---> stored  |||||||||||||||||||||||| mqtt failed mqtt data store localy\n");
      } 
      else
      {
	printf("---> dropped\n");
      }
    } 
    else
    {
      printf("---> throttled\n");
    }
    return -1;
  }

  last_offline_store_time = 0;
  printf("[Published] %s\n", topic);
  return 0;
}

// ADD THIS NEW FUNCTION
/*int MQTT_Yield(MQTTAsync* client, int timeout_ms)
  {
  return MQTTAsyn_Yield(client, timeout_ms);
  } */
