#ifndef MQTT_H
#define MQTT_H

#include <MQTTClient.h>

void store_message(const char *topic, const char *payload);
void forward_stored_messages(MQTTClient *client);
int MQTT_Init(MQTTClient *client);
int MQTT_Connect(MQTTClient *client, char *username, char *password);
void MQTT_Disconnect(MQTTClient *client);
int MQTT_Publish(MQTTClient *client, char *payload, char *topic);

#endif
