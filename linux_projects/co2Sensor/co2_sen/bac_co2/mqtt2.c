#include "mqtt.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ADDRESS "zedbee.io" // zedbee cloude name
#define CLIENTID "DEV_ZBDID02BA2CA259E4" // Device MAC ID

#define QOS 0
#define TIMEOUT 10000L

#define DATA_STORE_FILE "mqtt_store_data.txt"    // store and forward  

// typedef void MQTTClient_deliveryComplete(void *context, MQTTClient_deliveryToken dt)
void deliveryCompleted(void *context, MQTTClient_deliveryToken dt)
{
  printf("Message with token %d delivered\n", dt);
}

// typedef void MQTTClient_connectionLost(void *context, char *cause)
void connLostCb(void *context, char *cause)
{
  printf("Connection lost : %s\n", cause ? cause : "unknown");
}

int subscribeCallbacks(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
  printf("Mesaage on topic %s: %s\n", topicName, (char *)message->payload);

  // void MQTTClient_freeMessage(MQTTClient_message ** msg)
  MQTTClient_freeMessage(&message);

  // void MQTTClient_free(void * ptr)
  MQTTClient_free(topicName);
  return 1;
}

/* mqtt when failed, real time mqtt data timestamp --> data store and forwared */
void store_message(const char *topic, const char *payload)
{
  FILE *fp = fopen(DATA_STORE_FILE, "a");
  if (!fp)
  {
    perror("store_message fopen");
    return;
  }

  // times 
  time_t now = time(NULL);
  fprintf(fp, "%ld|%s|%s\n", now, topic, payload); 
  fclose(fp);
  printf("Stored locally (topic=%s)\n", topic);
}

/* forward */
void forward_stored_messages(MQTTClient *client)
{
  FILE *fp = fopen(DATA_STORE_FILE, "r");
  if(!fp)
  {
    return;
  }

  FILE *tmp = fopen("tmp_store.txt", "w");
  if (!tmp)
  {
    fclose(fp);
    return;
  }

  char line[2048];

  while(fgets(line, sizeof(line), fp))
  {
    char *saveptr;
    char *ts = strtok_r(line, "|", &saveptr);
    char *topic = strtok_r(NULL, "|", &saveptr);
    char *payload = strtok_r(NULL, "\n", &saveptr);

    if (!topic || !payload)
    {
      continue;
    }

    int rc = MQTT_Publish(client, payload, topic);
    if (rc != 0)
    {
      //  failed keep in temp file
      fprintf(tmp, "%s|%s|%s\n", ts, topic, payload);
    }
    else
    {
      printf("Forwared stored msg to topic '%s': %s\n\n", topic, payload);
      log_data("Forwared stored msg to topic '%s': %s\n\n", topic, payload);
    }
  }
  fclose(fp);
  fclose(tmp);

  remove(DATA_STORE_FILE);
  rename("tmp_store.txt", DATA_STORE_FILE);
}



int MQTT_Init(MQTTClient *client)
{
  // int MQTTClient_create(MQTTClient * handle, const char * serverURI, const char * clientId, int persistence_type, void * persistence_context)
  int rc = MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    printf("Failed to create MQTT client: %d\n", rc);
    return -1;
  }
  return 0;
}

int MQTT_Connect(MQTTClient *client, char *username, char *password)
{
  // MQTTClient_connectOptions_initializer
  // #define MQTTClient_connectOptions_initializer { {'M', 'Q', 'T', 'C'}, 6, 60, 1, 1, NULL, NULL, NULL, 30, 0, NULL, 0, NULL, MQTTVERSION_DEFAULT, {NULL, 0, 0}, {0, NULL}, -1, 0}
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  conn_opts.keepAliveInterval = 20; // keep interval time
  conn_opts.cleansession = 1;
  conn_opts.username = username; // username
  conn_opts.password = password; // password
  int rc;

  // int MQTTClient_setCallbacks(MQTTClient handle, void * context, MQTTClient_connectionLost * cl, MQTTClient_messageArrived * ma, MQTTClient_deliveryComplete * dc)
  if ((rc = MQTTClient_setCallbacks(*client, NULL, connLostCb, subscribeCallbacks, deliveryCompleted)) != MQTTCLIENT_SUCCESS)
  {
    printf("Failed to set callbacks: %d\n", rc);
    return -1;
  }

  // int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions * options)
  if ((rc = MQTTClient_connect(*client, &conn_opts)) != MQTTCLIENT_SUCCESS)
  {
    printf("=====> *** Failed to connect to MQTT broker *** <===== : %d\n\n", rc);
    return -1;
  }
  else
  {
    printf("MQTT connection success\n");
    
    // Try forwarding stored message after reconnect
    forward_stored_messages(client);    /*------------------------------*/
  }
  return 0;
}

void MQTT_Disconnect(MQTTClient *client)
{
  // int MQTTClient_disconnect (MQTTClient handle, int timeout)
  MQTTClient_disconnect(*client, TIMEOUT);

  // void MQTTClient_destroy(MQTTClient * handle)
  MQTTClient_destroy(client);
}

int MQTT_Publish(MQTTClient *client, char *payload, char *topic)
{
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
    printf("Publish failed: %d\n", rc);

    //  Store message locally instead of dropping
    store_message(topic, payload);           /*------------------------------*/

    return -1;
  }
  else
  {
    /* this print for mqtt publish data */
    printf("Publish to topic '%s' : %s\n\n", topic, payload);
    log_data("Publish to topic '%s': %s\n\n", topic, payload);
    return 0;
  }
}
