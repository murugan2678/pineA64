#ifndef MQTT_H
#define MQTT_H

#include "config.h"	//  config.h the header files
#include "data.h"	//  data.h the header files
#include <MQTTAsync.h>
#include <stdint.h>
#include <time.h>

//  structure for bms commands give that take to write to modbus address
typedef struct
{
  modbus_t *modbus_ctx;
  MQTTAsync *mqtt_client;
} CommandContext;


//  this structure declare for cmd_ctx
extern CommandContext cmd_ctx;
extern time_t publish_interval_sec;

extern int mqtt_is_connected;


void MQTT_Init(MQTTAsync *client);
int8_t MQTT_Connect(MQTTAsync *client, const char *username, const char *password);
void MQTT_Disconnect(MQTTAsync *client);
int8_t MQTT_Publish(MQTTAsync *client, const char *topic, const char *payload);

// Called from onConnect success callback
void MQTT_Subscribe(void *context, MQTTAsync_successData *response);

// Offline message handling
void store_message(const char *topic, const char *payload);
void forward_stored_messages(MQTTAsync *client);
void load_publish_interval(void);

//int MQTT_Yield(MQTTAsync* client, int timeout_ms);  // ← ADD THIS

#endif
