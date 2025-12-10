#include "config.h"       // read_config_file
#include "co2_client.h"   // co2_client_thread
#include "server.h"       // server_thread
#include "data.h"         // sensorData struct
#include "jsonify.h"      // createMainPkt_B
#include "mqtt.h"         // MQTT functions
#include "boodskap.h"     // Boodskap platform functions
#include <pthread.h>
#include <modbus/modbus.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#define MQTT_TOPIC "/BZHEZISEWY/device/ZBDID02BA2CA259E4/msgs/Gateway/1/101"
#define MQTT_USERNAME ""  // Empty for test broker (broker.hivemq.com)
#define MQTT_PASSWORD ""  // Empty for test broker
#define BUFFER_SIZE 2048
#ifndef PUBLISH_INTERVAL
#define PUBLISH_INTERVAL 300  // 5 minutes for publishing
#endif
#ifndef RECONNECT_INTERVAL
#define RECONNECT_INTERVAL 30  // 30 seconds for reconnection checks
#endif
#ifndef MAX_BACKOFF
#define MAX_BACKOFF 300  // Max sleep on reconnection failure (5 minutes)
#endif

// MQTT thread function
void *mqtt_thread(void *arg) {
    printf("MQTT thread started\n");
    int seq = 0;
    MQTTClient client;
    modbus_t *local_ctx = NULL;
    int connected = 0;
    int retry_count = 0;
    time_t last_reconnect_attempt = 0;
    time_t last_publish_time = 0;

    // Print current working directory for file debugging
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    } else {
        fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
    }

    if (MQTT_Init(&client) != 0) {
        fprintf(stderr, "Failed to initialize MQTT client\n");
        return NULL;
    }

    // Initial connection attempt
    if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) == 0) {
        connected = 1;
        printf("Initial MQTT connection successful\n");
    } else {
        fprintf(stderr, "Initial MQTT connection failed\n");
    }

    local_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (local_ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        if (connected) MQTT_Disconnect(&client);
        return NULL;
    }

    if (modbus_set_slave(local_ctx, SERVER_SLAVE_ID) == -1) {
        fprintf(stderr, "Invalid slave ID: %s\n", modbus_strerror(errno));
        modbus_free(local_ctx);
        if (connected) MQTT_Disconnect(&client);
        return NULL;
    }

    modbus_set_response_timeout(local_ctx, 1, 0);
    if (modbus_connect(local_ctx) == -1) {
        fprintf(stderr, "Initial Modbus connection failed: %s\n", modbus_strerror(errno));
    }

    time_t last_log_time = time(NULL);
    while (1) {
        time_t current_time = time(NULL);

        // Log activity every 30 seconds
        if (current_time - last_log_time >= 30) {
            printf("MQTT thread is active. Connected: %s. Current time: %s", connected ? "Yes" : "No", ctime(&current_time));
            last_log_time = current_time;
        }

        // Frequent reconnection checks if disconnected (every RECONNECT_INTERVAL seconds)
        if (!connected && (current_time - last_reconnect_attempt >= RECONNECT_INTERVAL)) {
            retry_count++;
            int backoff = (int)(10 * pow(2, retry_count - 1));  // Exponential: 10s, 20s, 40s, ...
            if (backoff > MAX_BACKOFF) backoff = MAX_BACKOFF;
            printf("Reconnection attempt #%d (backoff: %ds)\n", retry_count, backoff);

            if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) == 0) {
                connected = 1;
                retry_count = 0;  // Reset on success
                printf("MQTT reconnected successfully after %d attempts\n", retry_count);
            } else {
                fprintf(stderr, "Reconnection failed (attempt %d/%d)\n", retry_count, backoff);
                last_reconnect_attempt = current_time;
            }
        }

        // Publish only every PUBLISH_INTERVAL seconds (independent of reconnection)
        if (current_time - last_publish_time >= PUBLISH_INTERVAL) {
            printf("Starting publish cycle...\n");

            // Read Modbus registers
            uint16_t reg[20];
            int num_read = modbus_read_registers(local_ctx, 0, 20, reg);
            if (num_read == -1) {
                fprintf(stderr, "Failed to read Modbus registers: %s\n", modbus_strerror(errno));
                modbus_close(local_ctx);
                sleep(5);
                if (modbus_connect(local_ctx) == -1) {
                    fprintf(stderr, "Modbus reconnection failed: %s\n", modbus_strerror(errno));
                }
                last_publish_time = current_time;  // Skip publish but update time
                continue;
            }

            // Print register data
            for (int i = 0; i < num_read; i++) {
                printf("main.c mqtt send data buffer reg[%d] = %d\n", i, reg[i]);
            }

            // Populate sensor data
            sensorData sensors[2];
            sensors[0].carbonDioxide = reg[0];
            sensors[0].temperature = reg[1] / 100.0;
            sensors[0].humidity = reg[2];
            sensors[0].pressureTempFive = reg[3];
            sensors[0].pressureTemp = reg[4];
            sensors[1].carbonDioxide = reg[10];
            sensors[1].temperature = reg[11] / 100.0;
            sensors[1].humidity = reg[12];
            sensors[1].pressureTempFive = reg[13];
            sensors[1].pressureTemp = reg[14];

            // Create JSON payload
            char json_buffer[BUFFER_SIZE];
            createMainPkt_B(json_buffer, seq++, current_time, sensors, 2);

            // Attempt publish
            if (MQTT_Publish(&client, json_buffer, MQTT_TOPIC) != 0) {
                fprintf(stderr, "Publish failed - data stored locally\n");
                if (connected) {
                    connected = 0;  // Mark disconnected on publish failure
                    modbus_close(local_ctx);
                    MQTT_Disconnect(&client);
                }
            } else {
                printf("Publish successful\n");
            }

            last_publish_time = current_time;
        }

        // Sleep briefly to avoid busy loop (1 second)
        sleep(1);
    }

    // Cleanup
    modbus_close(local_ctx);
    modbus_free(local_ctx);
    if (connected) MQTT_Disconnect(&client);
    return NULL;
}

// Main function (unchanged except for cwd print)
int main() {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Main: Current working directory: %s\n", cwd);
    } else {
        fprintf(stderr, "Main: getcwd failed: %s\n", strerror(errno));
    }

    if (read_config_file() != 0) {
        fprintf(stderr, "Failed to read configuration file\n");
        return 1;
    }

    pthread_t co2_tid, server_tid, mqtt_tid;
    if (pthread_create(&co2_tid, NULL, co2_client_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create CO2 client thread\n");
        return 1;
    }
    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create server thread\n");
        return 1;
    }
    if (pthread_create(&mqtt_tid, NULL, mqtt_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create MQTT thread\n");
        return 1;
    }

    pthread_join(co2_tid, NULL);
    pthread_join(server_tid, NULL);
    pthread_join(mqtt_tid, NULL);
    return 0;
}
