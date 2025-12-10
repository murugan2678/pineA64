#include "config.h"
#include "co2_client.h"
#include "server.h"
#include "data.h"
#include "jsonify.h"
#include "mqtt.h"
#include "boodskap.h"
#include <pthread.h>
#include <modbus/modbus.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MQTT_TOPIC "/BZHEZISEWY/device/ZBDID02BA2CA259E4/msgs/Gateway/1/101"
#define MQTT_USERNAME "DEV_BZHEZISEWY"
#define MQTT_PASSWORD "UUOuKQvTSUtv"
#define BUFFER_SIZE 2048
#ifndef PUBLISH_INTERVAL
#define PUBLISH_INTERVAL 300
#endif

void *mqtt_thread(void *arg) {
    printf("MQTT thread started\n");
    int seq = 0;
    MQTTClient client;
    modbus_t *local_ctx = NULL;
    int connected = 0;

    if (MQTT_Init(&client) != 0) {
        fprintf(stderr, "Failed to initialize MQTT client\n");
        return NULL;
    }

    // Print current working directory for debugging
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    } else {
        fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
    }

    if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) == 0) {
        connected = 1;
    } else {
        fprintf(stderr, "Initial MQTT connection failed\n");
    }

    local_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (local_ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        if (connected) {
            MQTT_Disconnect(&client);
        }
        return NULL;
    }

    if (modbus_set_slave(local_ctx, SERVER_SLAVE_ID) == -1) {
        fprintf(stderr, "Invalid slave ID: %s\n", modbus_strerror(errno));
        modbus_free(local_ctx);
        if (connected) {
            MQTT_Disconnect(&client);
        }
        return NULL;
    }

    modbus_set_response_timeout(local_ctx, 1, 0);
    if (modbus_connect(local_ctx) == -1) {
        fprintf(stderr, "Initial Modbus connection failed: %s\n", modbus_strerror(errno));
    }

    time_t last_log_time = time(NULL);
    while (1) {
        printf("MQTT thread loop iteration\n");
        time_t current_time = time(NULL);
        if (current_time - last_log_time >= 30) {
            printf("MQTT thread is active. Current time: %s", ctime(&current_time));
            last_log_time = current_time;
        }

        // Attempt reconnection if not connected
        if (!connected) {
            if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) == 0) {
                connected = 1;
                printf("MQTT reconnected successfully\n");
            } else {
                fprintf(stderr, "MQTT reconnection failed\n");
                // Continue to try publishing to trigger store_message
            }
        }

        uint16_t reg[20];
        int num_read = modbus_read_registers(local_ctx, 0, 20, reg);
        if (num_read == -1) {
            fprintf(stderr, "Failed to read Modbus registers: %s\n", modbus_strerror(errno));
            modbus_close(local_ctx);
            sleep(5);
            if (modbus_connect(local_ctx) == -1) {
                fprintf(stderr, "Modbus reconnection failed: %s\n", modbus_strerror(errno));
            }
            continue;
        }

        for (int i = 0; i < num_read; i++) {
            printf("main.c mqtt send data buffer reg[%d] = %d\n", i, reg[i]);
        }

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

        char json_buffer[BUFFER_SIZE];
        createMainPkt_B(json_buffer, seq++, current_time, sensors, 2);

        // Attempt publish regardless of connection status
        if (MQTT_Publish(&client, json_buffer, MQTT_TOPIC) != 0) {
            fprintf(stderr, "Failed to publish sensor data\n");
            connected = 0; // Mark as disconnected
            modbus_close(local_ctx);
            MQTT_Disconnect(&client);
        } else {
            printf("Published to topic '%s': %s\n", MQTT_TOPIC, json_buffer);
        }

        sleep(PUBLISH_INTERVAL);
    }

    modbus_close(local_ctx);
    modbus_free(local_ctx);
    if (connected) {
        MQTT_Disconnect(&client);
    }
    return NULL;
}

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
