#include "mqtt.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define ADDRESS "zedbee.io"	 // zedbee cloude name
#define CLIENTID "DEV_ZBDID02BA2CA259E4"	// Device MAC ID

#define QOS 0
#define TIMEOUT 10000L

#define DATA_STORE_FILE "mqtt_store_data.txt"	// store and forward
#define TEMP_STORE_FILE "tmp_store.txt"

// typedef void MQTTClient_deliveryComplete(void *context, MQTTClient_deliveryToken dt)
void deliveryCompleted(void *context, MQTTClient_deliveryToken dt) 
{
  printf("Message with token %d delivered\n", dt);
}

// typedef void MQTTClient_connectionLost(void *context, char *cause)
void connLostCb(void *context, char *cause) 
{
  printf("Connection lost: %s\n", cause ? cause : "unknown");
}

int subscribeCallbacks(void *context, char *topicName, int topicLen, MQTTClient_message *message) 
{
  printf("Message on topic %s: %s\n", topicName, (char *)message->payload);

  // void MQTTClient_freeMessage(MQTTClient_message ** msg)
  MQTTClient_freeMessage(&message);

  // void MQTTClient_free(void * ptr)
  MQTTClient_free(topicName);
  return 1;
}

/* mqtt when failed, real time mqtt data timestamp --> data store and forwared */
void store_message(const char *topic, const char *payload) 
{
  printf("Attempting to store message\n");
  // printf("Attempting to store message (topic=%s)\n", topic);

  /* mqtt_store_data.txt this file create and flags for write only. file permission for 0644 6 ---> rw read and write, 4 ---> r read, 4 ---> r read */
  //  int open(const char *pathname, int flags, mode_t mode);
  int fd = open(DATA_STORE_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
  if (fd == -1)
  {
    fprintf(stderr, "store_message open failed: %s\n", strerror(errno));
    return;
  }

  /* fd ---> file descriptor pass to fdopen and "a" ---> Open for appending (writing at end of file) */
  //  FILE *fdopen(int fd, const char *mode);
  FILE *fp = fdopen(fd, "a");
  if (!fp) 
  {
    fprintf(stderr, "store_message fdopen failed: %s\n", strerror(errno));
    // int close(int fd);
    close(fd);  // close for fd
    return;
  }

  //  curretnt times taken
  time_t now = time(NULL);   

  //  int fprintf(FILE *restrict stream, const char *restrict format, ...);
  if (fprintf(fp, "%ld|%s|%s\n", now, topic, payload) < 0) 
  {
    fprintf(stderr, "store_message fprintf failed: %s\n", strerror(errno));
  }

  //  int fclose(FILE *stream); ,  fclose the fp 
  if (fclose(fp) != 0) 
  {
    fprintf(stderr, "store_message fclose failed: %s\n", strerror(errno));
  }
  else
  {
    printf("Stored locally topic '%s': %s\n", topic, payload);
  }
}

/* forward */
void forward_stored_messages(MQTTClient *client) 
{
  printf("Attempting to forward stored messages\n");

  /* open file for mqtt_store_data.txt, falgs set for read only */
  //  int open(const char *pathname, int flags);
  int fd = open(DATA_STORE_FILE, O_RDONLY);
  if (fd == -1) 
  {
    printf("\n************ No stored messages to forward (file does not exist)*************\n\n");
    return;
  }
  
  /* fd open and set for permission mode for "r" read only */
  //  FILE *fdopen(int fd, const char *mode);
  FILE *fp = fdopen(fd, "r");
  if (!fp) 
  {
    fprintf(stderr, "*****forward_stored_messages fdopen failed: %s\n", strerror(errno));
    close(fd);
    return;
  }

  /* open and create file for tmp_store.txt, flags set for create and write, truncate. file permission set for 0644 */
  //  int open(const char *pathname, int flags, mode_t mode);
  int temp_fd = open(TEMP_STORE_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (temp_fd == -1) 
  {
    fprintf(stderr, "forward_stored_messages temp open failed: %s\n", strerror(errno));
    fclose(fp);
    return;
  }
  else 
  {
    printf("open the temp file success **********\n\n");
  }

  /* fdopen file for temp_fd, mode permission for "w" write only */
  //  FILE *fdopen(int fd, const char *mode);
  FILE *temp_fp = fdopen(temp_fd, "w");
  if (!temp_fp) 
  {
    fprintf(stderr, "forward_stored_messages temp fdopen failed: %s\n", strerror(errno));
    fclose(fp);
    close(temp_fd);
    return;
  }

  char line[2048];   //  character buffer size store for 2048

  /* fgets ---> input of characters and strings */
  //  char *fgets(char s[restrict .size], int size, FILE *restrict stream);
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

    // MQTTClient_message_initializer
    // #define MQTTClient_message_initializer { {'M', 'Q', 'T', 'M'}, 1, 0, NULL, 0, 0, 0, 0, MQTTProperties_initializer }
    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = payload;
    msg.payloadlen = strlen(payload);
    msg.qos = QOS;
    msg.retained = 0;

    //  when mqtt connect failed that time publis data message printing
    
    // int MQTTClient_publishMessage (MQTTClient handle, const char * topicName, MQTTClient_message * msg, MQTTClient_deliveryToken * dt)
    int rc = MQTTClient_publishMessage(*client, topic, &msg, NULL);
    if (rc != MQTTCLIENT_SUCCESS) 
    {
      //  failed keep in temp file
      fprintf(temp_fp, "%s|%s|%s\n", ts, topic, payload);
      printf("Failed to forward stored message to topic '%s': %s\n\n", topic, payload);
    }
    else 
    {
      //****** after reconnect the mqtt connnect this data publish data ******//
      printf("*****Forwarded stored message to topic***** '%s': %s\n\n", topic, payload);   // this printf for output printing
      log_data("Forwared stored message topic '%s': %s\n", topic, payload); 	// this for log_data print for log directory printing
    }
  }

  //  fclose ---> close a stream
  fclose(fp);    
  fclose(temp_fp);

  //  rename the "mqtt_store_data.txt" this file rename to  "tmp_store.txt",  change the name or location of a file
  //  int rename(const char *oldpath, const char *newpath);
  if (rename(TEMP_STORE_FILE, DATA_STORE_FILE) != 0) 
  {
    fprintf(stderr, "forward_stored_messages rename failed: %s\n", strerror(errno));
  }
}

int MQTT_Init(MQTTClient *client) 
{
  // int MQTTClient_create(MQTTClient * handle, const char * serverURI, const char * clientId, int persistence_type, void * persistence_context)
  int rc = MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  if (rc != MQTTCLIENT_SUCCESS) 
  {
    fprintf(stderr, "Failed to create MQTT client: %d %s\n", rc, MQTTClient_strerror(rc));
    return -1;
  }
  return 0;
}

int MQTT_Connect(MQTTClient *client, char *username, char *password) 
{
  if (*client == NULL)
  {
    fprintf(stderr, "Invalid MQTT client handle\n");
    return -1;
  }

  // MQTTClient_connectOptions_initializer
  // #define MQTTClient_connectOptions_initializer { {'M', 'Q', 'T', 'C'}, 6, 60, 1, 1, NULL, NULL, NULL, 30, 0, NULL, 0, NULL, MQTTVERSION_DEFAULT, {NULL, 0, 0}, {0, NULL}, -1, 0}
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  conn_opts.keepAliveInterval = 20;  // keep interval time
  conn_opts.cleansession = 1;  
  conn_opts.username = username;  // username
  conn_opts.password = password;  // password

  int rc;

  // int MQTTClient_setCallbacks(MQTTClient handle, void * context, MQTTClient_connectionLost * cl, MQTTClient_messageArrived * ma, MQTTClient_deliveryComplete * dc)
  if ((rc = MQTTClient_setCallbacks(*client, NULL, connLostCb, subscribeCallbacks, deliveryCompleted)) != MQTTCLIENT_SUCCESS) 
  {
    fprintf(stderr, "Failed to set callbacks: %d %s\n", rc, MQTTClient_strerror(rc));
    return -1;
  }

  // int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions * options)
  if ((rc = MQTTClient_connect(*client, &conn_opts)) != MQTTCLIENT_SUCCESS) 
  {
    fprintf(stderr, "=====> *** Failed to connect to MQTT broker *** <===== : %d (%s)\n", rc, MQTTClient_strerror(rc));
    return -1;
  }
  else
  {
    printf("MQTT connection success\n");

    // Try forwarding stored message after reconnect
    forward_stored_messages(client);
  }

  return 0;
}

void MQTT_Disconnect(MQTTClient *client) 
{
  if (*client != NULL)
  {
    // int MQTTClient_disconnect (MQTTClient handle, int timeout)
    int rc = MQTTClient_disconnect(*client, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS)
    {
      fprintf(stderr, "Failed to disconnect Mqtt client: %d %s\n", rc, MQTTClient_strerror(rc));
    }
    else
    {
      printf("MQTT client disconnected successfully\n");
    }
  }
  // void MQTTClient_destroy(MQTTClient * handle)
  //MQTTClient_destroy(client);
}

int MQTT_Publish(MQTTClient *client, char *payload, char *topic) 
{
  // printf("Attempting to publish to topic '%s': %s\n", topic, payload);

  // MQTTClient_message_initializer
  // #define MQTTClient_message_initializer { {'M', 'Q', 'T', 'M'}, 1, 0, NULL, 0, 0, 0, 0, MQTTProperties_initializer }
  MQTTClient_message msg = MQTTClient_message_initializer;
  msg.payload = payload;
  msg.payloadlen = strlen(payload);
  msg.qos = QOS;
  msg.retained = 0;
  int rc;

  /* this function mqtt publish data */
  // int MQTTClient_publishMessage (MQTTClient handle, const char * topicName, MQTTClient_message * msg, MQTTClient_deliveryToken * dt)
  if ((rc = MQTTClient_publishMessage(*client, topic, &msg, NULL)) != MQTTCLIENT_SUCCESS) 
  {
    fprintf(stderr, "Publish failed: %d %s\n", rc, MQTTClient_strerror(rc));

    //  Store message locally instead of dropping
    store_message(topic, payload);
    return -1;
  }
  else
  {
    /* this print for mqtt publish data */
    printf("Published to topic '%s': %s\n", topic, payload);
    log_data("Published to topic '%s': %s\n", topic, payload);
  }
  return 0;
}
