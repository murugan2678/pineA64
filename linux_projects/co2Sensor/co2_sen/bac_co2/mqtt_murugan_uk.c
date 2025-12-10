#include "mqtt.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define ADDRESS "zedbee.io"
#define CLIENTID "DEV_ZBDID02BA2CA259E4"
#define QOS 0
#define TIMEOUT 10000L
#define DATA_STORE_FILE "mqtt_store_data.txt"
#define TEMP_STORE_FILE "tmp_store.txt"

void deliveryCompleted(void *context, MQTTClient_deliveryToken dt) {
    printf("Message with token %d delivered\n", dt);
}

void connLostCb(void *context, char *cause) {
    printf("Connection lost: %s\n", cause ? cause : "unknown");
}

int subscribeCallbacks(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("Message on topic %s: %s\n", topicName, (char *)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void store_message(const char *topic, const char *payload) {
    printf("Attempting to store message (topic=%s)\n", topic);
    int fd = open(DATA_STORE_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd == -1) {
        fprintf(stderr, "store_message open failed: %s\n", strerror(errno));
        return;
    }
    FILE *fp = fdopen(fd, "a");
    if (!fp) {
        fprintf(stderr, "store_message fdopen failed: %s\n", strerror(errno));
        close(fd);
        return;
    }
    time_t now = time(NULL);
    if (fprintf(fp, "%ld|%s|%s\n", now, topic, payload) < 0) {
        fprintf(stderr, "store_message fprintf failed: %s\n", strerror(errno));
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "store_message fclose failed: %s\n", strerror(errno));
    }
    printf("Stored locally (topic=%s): %s\n", topic, payload);
}

void forward_stored_messages(MQTTClient *client) {
    printf("Attempting to forward stored messages\n");
    int fd = open(DATA_STORE_FILE, O_RDONLY);
    if (fd == -1) {
        printf("\n************ No stored messages to forward (file does not exist)*************\n\n");
        return;
    }
    FILE *fp = fdopen(fd, "r");
    if (!fp) {
        fprintf(stderr, "*****forward_stored_messages fdopen failed: %s\n", strerror(errno));
        close(fd);
        return;
    }
    int temp_fd = open(TEMP_STORE_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (temp_fd == -1) {
        fprintf(stderr, "forward_stored_messages temp open failed: %s\n", strerror(errno));
        fclose(fp);
        return;
    } else {
        printf("open the temp file success **********\n\n");
    }
    FILE *temp_fp = fdopen(temp_fd, "w");
    if (!temp_fp) {
        fprintf(stderr, "forward_stored_messages temp fdopen failed: %s\n", strerror(errno));
        fclose(fp);
        close(temp_fd);
        return;
    }
    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        char *saveptr;
        char *ts = strtok_r(line, "|", &saveptr);
        char *topic = strtok_r(NULL, "|", &saveptr);
        char *payload = strtok_r(NULL, "\n", &saveptr);
        if (!ts || !topic || !payload) {
            fprintf(temp_fp, "%s", line);
            continue;
        }
        MQTTClient_message msg = MQTTClient_message_initializer;
        msg.payload = payload;
        msg.payloadlen = strlen(payload);
        msg.qos = QOS;
        msg.retained = 0;
        int rc = MQTTClient_publishMessage(*client, topic, &msg, NULL);
        if (rc != MQTTCLIENT_SUCCESS) {
            fprintf(temp_fp, "%s|%s|%s\n", ts, topic, payload);
            printf("Failed to forward stored message to topic '%s': %s\n", topic, payload);
        } else {
            printf("****************>Forwarded stored message to topic '%s': %s\n", topic, payload);
        }
    }
    fclose(fp);
    fclose(temp_fp);
    if (rename(TEMP_STORE_FILE, DATA_STORE_FILE) != 0) {
        fprintf(stderr, "forward_stored_messages rename failed: %s\n", strerror(errno));
    }
}

int MQTT_Init(MQTTClient *client) {
    int rc = MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to create MQTT client: %d (%s)\n", rc, MQTTClient_strerror(rc));
        return -1;
    }
    return 0;
}

int MQTT_Connect(MQTTClient *client, char *username, char *password) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = username;
    conn_opts.password = password;
    int rc;
    if ((rc = MQTTClient_setCallbacks(*client, NULL, connLostCb, subscribeCallbacks, deliveryCompleted)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to set callbacks: %d (%s)\n", rc, MQTTClient_strerror(rc));
        return -1;
    }
    if ((rc = MQTTClient_connect(*client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "=====> *** Failed to connect to MQTT broker *** <===== : %d (%s)\n", rc, MQTTClient_strerror(rc));
        return -1;
    }
    printf("MQTT connection success\n");
    forward_stored_messages(client);
    return 0;
}

void MQTT_Disconnect(MQTTClient *client) {
    MQTTClient_disconnect(*client, TIMEOUT);
    MQTTClient_destroy(client);
}

int MQTT_Publish(MQTTClient *client, char *payload, char *topic) {
    printf("Attempting to publish to topic '%s': %s\n", topic, payload);
    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = payload;
    msg.payloadlen = strlen(payload);
    msg.qos = QOS;
    msg.retained = 0;
    int rc;
    if ((rc = MQTTClient_publishMessage(*client, topic, &msg, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Publish failed: %d (%s)\n", rc, MQTTClient_strerror(rc));
        store_message(topic, payload);
        return -1;
    }
    printf("Published to topic '%s': %s\n", topic, payload);
    return 0;
}
