#include "mqtt.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#define ADDRESS "zedbee.io" // zedbee cloud name
#define CLIENTID "DEV_ZBDID02BA2CA259E4" // Device MAC ID
#define QOS 0
#define TIMEOUT 10000L
#define DATA_STORE_FILE "mqtt_store_data.txt" // store and forward
#define TEMP_STORE_FILE "tmp_store.txt"

// Log data to a file
void log_data(const char *format, ...) {
    va_list args;
    va_start(args, format);
    FILE *log_fp = fopen("mqtt_log.txt", "a");
    if (log_fp) {
        vfprintf(log_fp, format, args);
        fclose(log_fp);
    } else {
        fprintf(stderr, "Failed to open log file: %s\n", strerror(errno));
    }
    va_end(args);
}

// Callback for delivery completion
void deliveryCompleted(void *context, MQTTClient_deliveryToken dt) {
    printf("Message with token %d delivered\n", dt);
}

// Callback for connection lost
void connLostCb(void *context, char *cause) {
    printf("Connection lost: %s\n", cause ? cause : "unknown");
}

// Callback for incoming messages
int subscribeCallbacks(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("Message on topic %s: %s\n", topicName, (char *)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// Store message locally when MQTT publish fails
void store_message(const char *topic, const char *payload) {
    printf("Attempting to store message\n");

    int fd = open(DATA_STORE_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd == -1) {
        fprintf(stderr, "store_message open failed: %s\n", strerror(errno));
        return;
    }

    // Optional: File locking to prevent concurrent writes
    /*
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        fprintf(stderr, "store_message file lock failed: %s\n", strerror(errno));
        close(fd);
        return;
    }
    */

    FILE *fp = fdopen(fd, "a");
    if (!fp) {
        fprintf(stderr, "store_message fdopen failed: %s\n", strerror(errno));
        close(fd);
        return;
    }

    time_t now = time(NULL);
    if (fprintf(fp, "%ld|%s|%s\n", now, topic, payload) < 0) {
        fprintf(stderr, "store_message fprintf failed: %s\n", strerror(errno));
    } else {
        printf("Stored locally topic '%s': %s\n", topic, payload);
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "store_message fclose failed: %s\n", strerror(errno));
    }
}

// Forward stored messages after reconnection
void forward_stored_messages(MQTTClient *client) {
    printf("Attempting to forward stored messages\n");

    // Check if MQTT client is connected
    if (!MQTTClient_isConnected(*client)) {
        fprintf(stderr, "Cannot forward messages: MQTT client is not connected\n");
        return;
    }

    int fd = open(DATA_STORE_FILE, O_RDONLY);
    if (fd == -1) {
        printf("No stored messages to forward (file does not exist)\n");
        return;
    }

    FILE *fp = fdopen(fd, "r");
    if (!fp) {
        fprintf(stderr, "forward_stored_messages fdopen failed: %s\n", strerror(errno));
        close(fd);
        return;
    }

    int temp_fd = open(TEMP_STORE_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (temp_fd == -1) {
        fprintf(stderr, "forward_stored_messages temp open failed: %s\n", strerror(errno));
        fclose(fp);
        return;
    }

    FILE *temp_fp = fdopen(temp_fd, "w");
    if (!temp_fp) {
        fprintf(stderr, "forward_stored_messages temp fdopen failed: %s\n", strerror(errno));
        fclose(fp);
        close(temp_fd);
        return;
    }

    char line[4096]; // Increased buffer size
    while (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        char *saveptr;
        char *ts = strtok_r(line, "|", &saveptr);
        char *topic = strtok_r(NULL, "|", &saveptr);
        char *payload = strtok_r(NULL, "|", &saveptr);

        if (!ts || !topic || !payload) {
            fprintf(stderr, "Skipping malformed line: %s\n", line);
            fprintf(temp_fp, "%s\n", line); // Keep malformed line
            continue;
        }

        MQTTClient_message msg = MQTTClient_message_initializer;
        msg.payload = payload;
        msg.payloadlen = strlen(payload);
        msg.qos = QOS;
        msg.retained = 0;

        MQTTClient_deliveryToken dt;
        int rc = MQTTClient_publishMessage(*client, topic, &msg, &dt);
        if (rc != MQTTCLIENT_SUCCESS) {
            fprintf(stderr, "Failed to forward message to topic '%s': %s\n", topic, MQTTClient_strerror(rc));
            fprintf(temp_fp, "%s|%s|%s\n", ts, topic, payload);
        } else {
            // Wait for delivery confirmation
            rc = MQTTClient_waitForCompletion(*client, dt, TIMEOUT);
            if (rc != MQTTCLIENT_SUCCESS) {
                fprintf(stderr, "Delivery failed for topic '%s': %s\n", topic, MQTTClient_strerror(rc));
                fprintf(temp_fp, "%s|%s|%s\n", ts, topic, payload);
            } else {
                printf("Forwarded stored message to topic '%s': %s\n", topic, payload);
                log_data("Forwarded stored message topic '%s': %s\n", topic, payload);
            }
        }
    }

    fclose(fp);
    fclose(temp_fp);

    if (rename(TEMP_STORE_FILE, DATA_STORE_FILE) != 0) {
        fprintf(stderr, "forward_stored_messages rename failed: %s\n", strerror(errno));
    } else {
        printf("Successfully updated stored messages\n");
    }
}

// Initialize MQTT client
int MQTT_Init(MQTTClient *client) {
    int rc = MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to create MQTT client: %d %s\n", rc, MQTTClient_strerror(rc));
        return -1;
    }
    return 0;
}

// Connect to MQTT broker with retry logic
int MQTT_Connect(MQTTClient *client, char *username, char *password) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = username;
    conn_opts.password = password;

    int rc;
    if ((rc = MQTTClient_setCallbacks(*client, NULL, connLostCb, subscribeCallbacks, deliveryCompleted)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to set callbacks: %d %s\n", rc, MQTTClient_strerror(rc));
        return -1;
    }

    // Retry connection up to 3 times
    int retries = 3;
    int delay_seconds = 5;
    for (int i = 0; i < retries; i++) {
        rc = MQTTClient_connect(*client, &conn_opts);
        if (rc == MQTTCLIENT_SUCCESS) {
            printf("MQTT connection success\n");
            // Wait briefly to ensure connection stability
            sleep(1);
            // Forward stored messages
            forward_stored_messages(client);
            return 0;
        }
        fprintf(stderr, "Failed to connect to MQTT broker (attempt %d/%d): %d (%s)\n", 
                i + 1, retries, rc, MQTTClient_strerror(rc));
        sleep(delay_seconds);
    }

    fprintf(stderr, "Failed to connect after %d attempts\n", retries);
    return -1;
}

// Disconnect from MQTT broker
void MQTT_Disconnect(MQTTClient *client) {
    MQTTClient_disconnect(*client, TIMEOUT);
    MQTTClient_destroy(client);
}

// Publish message to MQTT broker
int MQTT_Publish(MQTTClient *client, char *payload, char *topic) {
    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = payload;
    msg.payloadlen = strlen(payload);
    msg.qos = QOS;
    msg.retained = 0;

    MQTTClient_deliveryToken dt;
    int rc = MQTTClient_publishMessage(*client, topic, &msg, &dt);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Publish failed: %d %s\n", rc, MQTTClient_strerror(rc));
        store_message(topic, payload);
        return -1;
    }

    // Wait for delivery confirmation
    rc = MQTTClient_waitForCompletion(*client, dt, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Delivery failed: %d %s\n", rc, MQTTClient_strerror(rc));
        store_message(topic, payload);
        return -1;
    }

    printf("Published to topic '%s': %s\n", topic, payload);
    log_data("Published to topic '%s': %s\n", topic, payload);
    return 0;
}
