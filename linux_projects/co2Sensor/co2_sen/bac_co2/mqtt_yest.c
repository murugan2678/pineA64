#include "mqtt.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define ADDRESS "zedbee.io"
#define CLIENTID "DEV_ZBDID02BA2CA259E4"
#define QOS 0
#define TIMEOUT 10000L
#define STORE_FILE "mqtt_store.txt" // local storage file
#define MAX_RETRY_ATTEMPTS 3 // Max reconnection attempts

// ===== Callbacks =====
void deliveryCompleted(void *context, MQTTClient_deliveryToken dt)
{
  printf("Message with token %d delivered\n", dt);
}

void connLostCb(void *context, char *cause)
{
  printf("Connection lost: %s\n", cause ? cause : "unknown");
}

int subscribeCallbacks(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
  printf("Message on topic %s: %s\n", topicName, (char *)message->payload);
  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);
  return 1;
}

// ===== Storage Helpers =====
/* void store_message(const char *topic, const char *payload)
   {
   FILE *fp = fopen(STORE_FILE, "a");
   if (!fp) {
   perror("store_message fopen");
   return;
   }
   time_t now = time(NULL);
   fprintf(fp, "%ld|%s|%s\n", now, topic, payload); // timestamp|topic|payload
   fclose(fp);
   printf("Stored locally (topic=%s, payload=%s)\n", topic, payload);
   } */
// mqtt.c (replace the store_message function, around line 28)
void store_message(const char *topic, const char *payload)
{
  printf("Attempting to store message: topic=%s, payload=%s\n", topic, payload);
  FILE *fp = fopen(STORE_FILE, "a");
  if (!fp) {
    perror("store_message fopen failed");
    return;
  }
  time_t now = time(NULL);
  fprintf(fp, "%ld|%s|%s\n", now, topic, payload);
  fclose(fp);
  printf("Stored locally (topic=%s, payload=%s)\n", topic, payload);
}

void forward_stored_messages(MQTTClient *client)
{
  FILE *fp = fopen(STORE_FILE, "r");
  if (!fp) {
    printf("No stored messages to forward\n");
    return;
  }
  FILE *tmp = fopen("tmp_store.txt", "w");
  if (!tmp) {
    fclose(fp);
    perror("forward_stored_messages tmp fopen");
    return;
  }
  char line[2048];
  while (fgets(line, sizeof(line), fp)) {
    char *saveptr;
    char *ts = strtok_r(line, "|", &saveptr);
    char *topic = strtok_r(NULL, "|", &saveptr);
    char *payload = strtok_r(NULL, "\n", &saveptr);
    if (!topic || !payload) continue;
    int rc = MQTT_Publish(client, payload, topic);
    if (rc != 0) {
      // Failed → keep in temp file
      fprintf(tmp, "%s|%s|%s\n", ts, topic, payload);
      printf("Failed to forward stored message to topic '%s': %s\n", topic, payload);
    } else {
      printf("Forwarded stored msg to topic '%s': %s\n", topic, payload);
      log_data("Forwarded stored msg to topic '%s': %s\n", topic, payload);
    }
  }
  fclose(fp);
  fclose(tmp);
  remove(STORE_FILE); // Remove original file
  rename("tmp_store.txt", STORE_FILE); // Rename temp file to original
}

// ===== API =====
int MQTT_Init(MQTTClient *client)
{
  int rc = MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  if (rc != MQTTCLIENT_SUCCESS) {
    printf("Failed to create MQTT client: %d\n", rc);
    return -1;
  }
  return 0;
}

int MQTT_Connect(MQTTClient *client, char *username, char *password, char *topic, char *payload)
{
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;
  conn_opts.username = username;
  conn_opts.password = password;
  int rc;
  if ((rc = MQTTClient_setCallbacks(*client, NULL, connLostCb, subscribeCallbacks, deliveryCompleted)) != MQTTCLIENT_SUCCESS) {
    printf("Failed to set callbacks: %d\n", rc);
    return -1;
  }
  int attempt = 0;
  while (attempt < MAX_RETRY_ATTEMPTS) {
    rc = MQTTClient_connect(*client, &conn_opts);
    if (rc == MQTTCLIENT_SUCCESS) {
      printf("MQTT connection success\n");
      // Try forwarding stored messages after reconnect
      forward_stored_messages(client);
      return 0;
    }
    printf("Failed to connect to MQTT broker (attempt %d/%d): %d\n", attempt + 1, MAX_RETRY_ATTEMPTS, rc);
    attempt++;
    sleep(2); // Wait before retrying
  }
  printf("=====> *** Failed to connect to MQTT broker after %d attempts *** <=====\n", MAX_RETRY_ATTEMPTS);
  // Store the message if provided
  if (topic && payload) {
    store_message(topic, payload);
  }
  return -1;
}

void MQTT_Disconnect(MQTTClient *client)
{
  MQTTClient_disconnect(*client, TIMEOUT);
  MQTTClient_destroy(client);
}

int MQTT_Publish(MQTTClient *client, char *payload, char *topic)
{
  MQTTClient_message msg = MQTTClient_message_initializer;
  msg.payload = payload;
  msg.payloadlen = strlen(payload);
  msg.qos = QOS;
  msg.retained = 0;
  int rc;
  if ((rc = MQTTClient_publishMessage(*client, topic, &msg, NULL)) != MQTTCLIENT_SUCCESS) {
    printf("Publish failed: %d\n", rc);
    // Store message locally instead of dropping
    store_message(topic, payload);
    return -1;
  } else {
    printf("Publish to topic '%s': %s\n", topic, payload);
    log_data("Publish to topic '%s': %s\n", topic, payload);
    return 0;
  }
}
