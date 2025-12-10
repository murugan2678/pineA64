#include "config.h"
#include "co2_client.h"
#include "server.h"
#include "data.h"
#include "jsonify.h"
#include "mqtt.h"
#include "boodskap.h"
#include <pthread.h>
#include <modbus/modbus.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MQTT_TOPIC "/BZHEZISEWY/device/ZBDID02BA2CA259E4/msgs/Gateway/1/101"
#define MQTT_USERNAME "DEV_BZHEZISEWY"
#define MQTT_PASSWORD "UUOuKQvTSUtv"
#define BUFFER_SIZE 2048

static pthread_t co2_tid, server_tid, mqtt_tid;
static MQTTClient mqtt_client;
static volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t server_started = 0;

void signal_handler(int sig)
{
    keep_running = 0;
    printf("Received SIGINT, shutting down...\n");
    fflush(stdout);

    // Publish final MQTT messages with zeroed data
    if (MQTT_Connect(&mqtt_client, MQTT_USERNAME, MQTT_PASSWORD) != 0) {
        log_error("Failed to reconnect MQTT for final publish");
        printf("Failed to reconnect MQTT for final publish\n");
        fflush(stdout);
    } else {
        sensorData zero_data = {0, 0.0, 0, 0, 0};
        char json_buffer[BUFFER_SIZE];
        int seq = 9999;

        // First sensor
        printf("Creating final MQTT message for sensor 1\n");
        fflush(stdout);
        createMainPkt_B(json_buffer, seq, time(NULL), &zero_data, sen_co2_id);
        printf("Attempting to publish final message to topic '%s': %s\n", MQTT_TOPIC, json_buffer);
        fflush(stdout);
        if (MQTT_Publish(&mqtt_client, json_buffer, MQTT_TOPIC) != 0) {
            log_error("Failed to publish final message for sensor 1");
            printf("Failed to publish final message for sensor 1\n");
            fflush(stdout);
        } else {
            printf("Final message for sensor 1 published successfully\n");
            fflush(stdout);
        }

        // Second sensor
        createMainPkt_B(json_buffer, seq + 1, time(NULL), &zero_data, sen2_co2_id);
        printf("Attempting to publish final message to topic '%s': %s\n", MQTT_TOPIC, json_buffer);
        fflush(stdout);
        if (MQTT_Publish(&mqtt_client, json_buffer, MQTT_TOPIC) != 0) {
            log_error("Failed to publish final message for sensor 2");
            printf("Failed to publish final message for sensor 2\n");
            fflush(stdout);
        } else {
            printf("Final message for sensor 2 published successfully\n");
            fflush(stdout);
        }

        sleep(1); // Ensure delivery
        MQTT_Disconnect(&mqtt_client);
        printf("MQTT client disconnected\n");
        fflush(stdout);
    }

    // Join threads
    void *retval;
    if (pthread_join(co2_tid, &retval) != 0) {
        log_error("Failed to join CO2 client thread: %s", strerror(errno));
        printf("Failed to join CO2 client thread: %s\n", strerror(errno));
        fflush(stdout);
    }
    if (pthread_join(server_tid, &retval) != 0) {
        log_error("Failed to join server thread: %s", strerror(errno));
        printf("Failed to join server thread: %s\n", strerror(errno));
        fflush(stdout);
    }
    if (pthread_join(mqtt_tid, &retval) != 0) {
        log_error("Failed to join MQTT thread: %s", strerror(errno));
        printf("Failed to join MQTT thread: %s\n", strerror(errno));
        fflush(stdout);
    }

    // Clean up Modbus server
    if (server_started) {
        cleanup_modbus_server();
    } else {
        printf("Server thread did not start, skipping Modbus cleanup\n");
        fflush(stdout);
    }

    // Forcefully close port 5503
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(sock);
            printf("Forced port %d release\n", SERVER_PORT);
            fflush(stdout);
        } else {
            printf("Failed to force port %d release: %s\n", SERVER_PORT, strerror(errno));
            fflush(stdout);
            close(sock);
        }
    }

    printf("Program shutdown complete\n");
    fflush(stdout);
#ifdef _WIN32
    WSACleanup();
#endif
    exit(0);
}

void *mqtt_thread(void *arg)
{
    int seq = 0;
    printf("MQTT thread: Initializing MQTT client\n");
    fflush(stdout);
    if (MQTT_Init(&mqtt_client) != 0) {
        log_error("Failed to initialize MQTT client");
        printf("Failed to initialize MQTT client\n");
        fflush(stdout);
        return NULL;
    }

    printf("MQTT thread: Connecting to MQTT broker\n");
    fflush(stdout);
    if (MQTT_Connect(&mqtt_client, MQTT_USERNAME, MQTT_PASSWORD) != 0) {
        log_error("Failed to connect to MQTT broker");
        printf("Failed to connect to MQTT broker\n");
        fflush(stdout);
        MQTT_Disconnect(&mqtt_client);
        return NULL;
    }

    modbus_t *local_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (local_ctx == NULL) {
        log_error("Unable to allocate libmodbus for local");
        printf("Unable to allocate libmodbus for local\n");
        fflush(stdout);
        MQTT_Disconnect(&mqtt_client);
        return NULL;
    }

    if (modbus_set_slave(local_ctx, SERVER_SLAVE_ID) == -1) {
        log_error("Invalid slave ID for local");
        printf("Invalid slave ID for local\n");
        fflush(stdout);
        modbus_free(local_ctx);
        MQTT_Disconnect(&mqtt_client);
        return NULL;
    }

    time_t last_log_time = time(NULL);
    while (keep_running) {
        time_t current_time = time(NULL);
        if (current_time - last_log_time >= 30) {
            log_data("MQTT thread is active. Current time: %s", ctime(&current_time));
            printf("MQTT thread is active at %s", ctime(&current_time));
            fflush(stdout);
            last_log_time = current_time;
        }

        printf("MQTT thread: Attempting to read Modbus registers\n");
        fflush(stdout);
        if (modbus_connect(local_ctx) == -1) {
            log_error("Local Modbus connection failed: %s", modbus_strerror(errno));
            printf("Local Modbus connection failed: %s\n", modbus_strerror(errno));
            fflush(stdout);
            sleep(10);
            continue;
        }

        uint16_t reg[20];
        int num_read = modbus_read_registers(local_ctx, 0, 20, reg);
        modbus_close(local_ctx);
        if (num_read == -1) {
            log_error("Failed to read local registers: %s", modbus_strerror(errno));
            printf("Failed to read local registers: %s\n", modbus_strerror(errno));
            fflush(stdout);
            sleep(10);
            continue;
        }

        sensorData first, second;
        first.carbonDioxide = reg[0];
        first.temperature = reg[1] / 100.0; // Scale temperature
        first.humidity = reg[2];
        first.pressureTempFive = reg[3];
        first.pressureTemp = reg[4];
        second.carbonDioxide = reg[10];
        second.temperature = reg[11] / 100.0; // Scale temperature
        second.humidity = reg[12];
        second.pressureTempFive = reg[13];
        second.pressureTemp = reg[14];

        // Publish first sensor
        char json_buffer1[BUFFER_SIZE];
        printf("MQTT thread: Creating JSON for first sensor\n");
        fflush(stdout);
        createMainPkt_B(json_buffer1, seq++, time(NULL), &first, sen_co2_id);
        printf("Attempting to publish to topic '%s': %s\n", MQTT_TOPIC, json_buffer1);
        fflush(stdout);
        if (MQTT_Publish(&mqtt_client, json_buffer1, MQTT_TOPIC) != 0) {
            log_error("Failed to publish first sensor");
            printf("Failed to publish first sensor\n");
            fflush(stdout);
            MQTT_Disconnect(&mqtt_client);
            if (MQTT_Connect(&mqtt_client, MQTT_USERNAME, MQTT_PASSWORD) != 0) {
                log_error("Failed to reconnect MQTT");
                printf("Failed to reconnect MQTT\n");
                fflush(stdout);
                sleep(10);
                continue;
            }
        } else {
            printf("Publish to topic '%s' successful\n", MQTT_TOPIC);
            fflush(stdout);
        }

        // Publish second sensor
        char json_buffer2[BUFFER_SIZE];
        printf("MQTT thread: Creating JSON for second sensor\n");
        fflush(stdout);
        createMainPkt_B(json_buffer2, seq++, time(NULL), &second, sen2_co2_id);
        printf("Attempting to publish to topic '%s': %s\n", MQTT_TOPIC, json_buffer2);
        fflush(stdout);
        if (MQTT_Publish(&mqtt_client, json_buffer2, MQTT_TOPIC) != 0) {
            log_error("Failed to publish second sensor");
            printf("Failed to publish second sensor\n");
            fflush(stdout);
            MQTT_Disconnect(&mqtt_client);
            if (MQTT_Connect(&mqtt_client, MQTT_USERNAME, MQTT_PASSWORD) != 0) {
                log_error("Failed to reconnect MQTT");
                printf("Failed to reconnect MQTT\n");
                fflush(stdout);
                sleep(10);
                continue;
            }
        } else {
            printf("Publish to topic '%s' successful\n", MQTT_TOPIC);
            fflush(stdout);
        }

        printf("MQTT thread: Sleeping for 5 minutes\n");
        fflush(stdout);
        for (int i = 0; i < 300 && keep_running; i++) {
            sleep(1); // Break sleep into 1-second intervals to check keep_running
        }
    }

    modbus_free(local_ctx);
    MQTT_Disconnect(&mqtt_client);
    printf("MQTT thread exited\n");
    fflush(stdout);
    return NULL;
}

int main()
{
    // Forcefully close port 5503 before startup
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(sock);
            printf("Forced port %d release before startup\n", SERVER_PORT);
            fflush(stdout);
        } else {
            printf("Failed to force port %d release before startup: %s\n", SERVER_PORT, strerror(errno));
            fflush(stdout);
            close(sock);
        }
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
        log_error("WSAStartup failed with error %d", WSAGetLastError());
        perror("WSAStartup failed with error %d", WSAGetLastError());
        WSACleanup();
        return 1;
    }
#endif

    if (read_config_file() != 0) {
        log_error("Failed to read configuration files");
        perror("Failed to read configuration files");
        printf("Failed to read configuration files\n");
        fflush(stdout);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Create threads
    if (pthread_create(&co2_tid, NULL, co2_client_thread, NULL) != 0) {
        log_error("Failed to create CO2 client thread");
        perror("Failed to create CO2 client thread");
        printf("Failed to create CO2 client thread\n");
        fflush(stdout);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        log_error("Failed to create server thread");
        perror("Failed to create server thread");
        printf("Failed to create server thread\n");
        fflush(stdout);
        keep_running = 0;
        pthread_join(co2_tid, NULL);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    server_started = 1; // Indicate server thread started

    if (pthread_create(&mqtt_tid, NULL, mqtt_thread, NULL) != 0) {
        log_error("Failed to create MQTT thread");
        perror("Failed to create MQTT thread");
        printf("Failed to create MQTT thread\n");
        fflush(stdout);
        keep_running = 0;
        pthread_join(co2_tid, NULL);
        pthread_join(server_tid, NULL);
        if (server_started) {
            cleanup_modbus_server();
        }
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Set up signal handler
    signal(SIGINT, signal_handler);

    printf("All threads created successfully\n");
    fflush(stdout);

    // Join threads
    pthread_join(co2_tid, NULL);
    pthread_join(server_tid, NULL);
    pthread_join(mqtt_tid, NULL);

    // Final cleanup
    if (server_started) {
        cleanup_modbus_server();
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
