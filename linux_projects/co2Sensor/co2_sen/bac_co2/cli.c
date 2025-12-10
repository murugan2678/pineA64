#include "server.h"
#include "co2_client.h"
#include <modbus/modbus.h>
#include <string.h> // For memset
#include <errno.h>
#include <unistd.h> // For sleep

#define MAX_CONNECT_RETRIES 3
#define RETRY_DELAY_SEC 5

void *co2_client_thread(void *arg)
{
    modbus_t *ctx = NULL;
    modbus_t *ctx1 = NULL;
    modbus_t *server_ctx = NULL;
    uint16_t reg[NUM_OF_REGISTERS];
    uint16_t reg1[NUM_OF_REGISTERS];
    uint16_t write_regs[NUM_OF_REGISTERS];
    uint16_t write_regs1[NUM_OF_REGISTERS];
    time_t last_log_time = time(NULL);
    int co2_device_index = -1;
    int co2_device_index1 = -1;
    int ctx_connected = 0;
    int ctx1_connected = 0;
    int server_ctx_connected = 0;

    // Find CO2 devices (ID 1 and 2)
    for (int i = 0; i < total_devices; i++) {
        if (devices[i].id == 1) {
            co2_device_index = i;
            printf("CO2 device ID 1 found at index %d, IP: %s, Port: %d, Slave ID: %d\n",
                   co2_device_index, devices[i].device_IP, devices[i].device_port_number, devices[i].device_slave_ID);
            log_data("CO2 device ID 1 found at index %d, IP: %s, Port: %d, Slave ID: %d",
                     co2_device_index, devices[i].device_IP, devices[i].device_port_number, devices[i].device_slave_ID);
        }
        if (devices[i].id == 2) {
            co2_device_index1 = i;
            printf("CO2 device ID 2 found at index %d, IP: %s, Port: %d, Slave ID: %d\n",
                   co2_device_index1, devices[i].device_IP, devices[i].device_port_number, devices[i].device_slave_ID);
            log_data("CO2 device ID 2 found at index %d, IP: %s, Port: %d, Slave ID: %d",
                     co2_device_index1, devices[i].device_IP, devices[i].device_port_number, devices[i].device_slave_ID);
        }
    }

    if (co2_device_index == -1) {
        printf("ERROR: CO2 device not found in configuration for ID 1\n");
        log_error("CO2 device not found in configuration for ID 1");
    }
    if (co2_device_index1 == -1) {
        printf("ERROR: CO2 device not found in configuration for ID 2\n");
        log_error("CO2 device not found in configuration for ID 2");
    }

    // Validate device configuration
    if (co2_device_index != -1 && (!devices[co2_device_index].device_IP || devices[co2_device_index].device_port_number <= 0)) {
        printf("ERROR: Invalid IP or port for CO2 ID 1\n");
        log_error("Invalid IP or port for CO2 ID 1");
        co2_device_index = -1;
    }
    if (co2_device_index1 != -1 && (!devices[co2_device_index1].device_IP || devices[co2_device_index1].device_port_number <= 0)) {
        printf("ERROR: Invalid IP or port for CO2 ID 2\n");
        log_error("Invalid IP or port for CO2 ID 2");
        co2_device_index1 = -1;
    }

    // Initialize Modbus contexts
    if (co2_device_index != -1) {
        ctx = modbus_new_tcp(devices[co2_device_index].device_IP, devices[co2_device_index].device_port_number);
        if (ctx == NULL) {
            printf("ERROR: Unable to allocate libmodbus for CO2 ID 1: %s\n", modbus_strerror(errno));
            log_error("Unable to allocate libmodbus for CO2 ID 1: %s", modbus_strerror(errno));
            co2_device_index = -1;
        }
    }

    if (co2_device_index1 != -1) {
        ctx1 = modbus_new_tcp(devices[co2_device_index1].device_IP, devices[co2_device_index1].device_port_number);
        if (ctx1 == NULL) {
            printf("ERROR: Unable to allocate libmodbus for CO2 ID 2: %s\n", modbus_strerror(errno));
            log_error("Unable to allocate libmodbus for CO2 ID 2: %s", modbus_strerror(errno));
            co2_device_index1 = -1;
            if (ctx) modbus_free(ctx);
        }
    }

    server_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (server_ctx == NULL) {
        printf("ERROR: Unable to allocate libmodbus for server: %s\n", modbus_strerror(errno));
        log_error("Unable to allocate libmodbus for server: %s", modbus_strerror(errno));
        if (ctx) modbus_free(ctx);
        if (ctx1) modbus_free(ctx1);
        return NULL;
    }

    // Set slave IDs
    if (ctx && modbus_set_slave(ctx, devices[co2_device_index].device_slave_ID) == -1) {
        printf("ERROR: Invalid slave ID for CO2 ID 1: %s\n", modbus_strerror(errno));
        log_error("Invalid slave ID for CO2 ID 1: %s", modbus_strerror(errno));
        modbus_free(ctx);
        ctx = NULL;
        co2_device_index = -1;
    }

    if (ctx1 && modbus_set_slave(ctx1, devices[co2_device_index1].device_slave_ID) == -1) {
        printf("ERROR: Invalid slave ID for CO2 ID 2: %s\n", modbus_strerror(errno));
        log_error("Invalid slave ID for CO2 ID 2: %s", modbus_strerror(errno));
        modbus_free(ctx1);
        ctx1 = NULL;
        co2_device_index1 = -1;
    }

    if (modbus_set_slave(server_ctx, SERVER_SLAVE_ID) == -1) {
        printf("ERROR: Invalid slave ID for server: %s\n", modbus_strerror(errno));
        log_error("Invalid slave ID for server: %s", modbus_strerror(errno));
        modbus_free(server_ctx);
        if (ctx) modbus_free(ctx);
        if (ctx1) modbus_free(ctx1);
        return NULL;
    }

    // Set timeouts (1 second)
    uint32_t to_sec = 1;
    uint32_t to_usec = 0;
    if (ctx && modbus_set_response_timeout(ctx, to_sec, to_usec) == -1) {
        printf("ERROR: Failed to set response timeout for CO2 ID 1: %s\n", modbus_strerror(errno));
        log_error("Failed to set response timeout for CO2 ID 1: %s", modbus_strerror(errno));
        modbus_free(ctx);
        ctx = NULL;
        co2_device_index = -1;
    }
    if (ctx1 && modbus_set_response_timeout(ctx1, to_sec, to_usec) == -1) {
        printf("ERROR: Failed to set response timeout for CO2 ID 2: %s\n", modbus_strerror(errno));
        log_error("Failed to set response timeout for CO2 ID 2: %s", modbus_strerror(errno));
        modbus_free(ctx1);
        ctx1 = NULL;
        co2_device_index1 = -1;
    }
    if (modbus_set_response_timeout(server_ctx, to_sec, to_usec) == -1) {
        printf("ERROR: Failed to set response timeout for server: %s\n", modbus_strerror(errno));
        log_error("Failed to set response timeout for server: %s", modbus_strerror(errno));
        modbus_free(server_ctx);
        if (ctx) modbus_free(ctx);
        if (ctx1) modbus_free(ctx1);
        return NULL;
    }

    // Connect to Modbus devices with retries
    if (ctx) {
        for (int retry = 0; retry < MAX_CONNECT_RETRIES; retry++) {
            if (modbus_connect(ctx) == 0) {
                ctx_connected = 1;
                printf("INFO: CO2 Modbus connected for ID 1\n");
                log_data("CO2 Modbus connected for ID 1");
                break;
            }
            printf("ERROR: CO2 Modbus connection failed for ID 1 (attempt %d/%d): %s\n",
                   retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
            log_error("CO2 Modbus connection failed for ID 1 (attempt %d/%d): %s",
                      retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
            sleep(RETRY_DELAY_SEC);
        }
        if (!ctx_connected) {
            printf("ERROR: Failed to connect to CO2 ID 1 after %d retries\n", MAX_CONNECT_RETRIES);
            log_error("Failed to connect to CO2 ID 1 after %d retries", MAX_CONNECT_RETRIES);
            modbus_free(ctx);
            ctx = NULL;
            co2_device_index = -1;
        }
    }

    if (ctx1) {
        for (int retry = 0; retry < MAX_CONNECT_RETRIES; retry++) {
            if (modbus_connect(ctx1) == 0) {
                ctx1_connected = 1;
                printf("INFO: CO2 Modbus connected for ID 2\n");
                log_data("CO2 Modbus connected for ID 2");
                break;
            }
            printf("ERROR: CO2 Modbus connection failed for ID 2 (attempt %d/%d): %s\n",
                   retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
            log_error("CO2 Modbus connection failed for ID 2 (attempt %d/%d): %s",
                      retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
            sleep(RETRY_DELAY_SEC);
        }
        if (!ctx1_connected) {
            printf("ERROR: Failed to connect to CO2 ID 2 after %d retries\n", MAX_CONNECT_RETRIES);
            log_error("Failed to connect to CO2 ID 2 after %d retries", MAX_CONNECT_RETRIES);
            modbus_free(ctx1);
            ctx1 = NULL;
            co2_device_index1 = -1;
        }
    }

    for (int retry = 0; retry < MAX_CONNECT_RETRIES; retry++) {
        if (modbus_connect(server_ctx) == 0) {
            server_ctx_connected = 1;
            printf("INFO: Server Modbus connected\n");
            log_data("Server Modbus connected");
            break;
        }
        printf("ERROR: Server Modbus connection failed (attempt %d/%d): %s\n",
               retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
        log_error("Server Modbus connection failed (attempt %d/%d): %s",
                  retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
        sleep(RETRY_DELAY_SEC);
    }
    if (!server_ctx_connected) {
        printf("ERROR: Failed to connect to server after %d retries\n", MAX_CONNECT_RETRIES);
        log_error("Failed to connect to server after %d retries", MAX_CONNECT_RETRIES);
        modbus_free(server_ctx);
        if (ctx) modbus_free(ctx);
        if (ctx1) modbus_free(ctx1);
        return NULL;
    }

    // Proceed only if at least one device or server is connected
    if (!ctx_connected && !ctx1_connected) {
        printf("ERROR: No devices connected, exiting thread\n");
        log_error("No devices connected, exiting thread");
        modbus_free(server_ctx);
        return NULL;
    }

    while (1) {
        time_t current_time = time(NULL);
        if (current_time - last_log_time >= 30) {
            printf("INFO: CO2 client thread is active. Current time: %s", ctime(&current_time));
            log_data("CO2 client thread is active. Current time: %s", ctime(&current_time));
            last_log_time = current_time;
        }

        // Read and process first sensor (ID 1)
        if (ctx_connected) {
            memset(write_regs, 0, sizeof(write_regs)); // Initialize to zero
            int num_read = modbus_read_registers(ctx, 0, NUM_OF_REGISTERS, reg);
            if (num_read == -1) {
                printf("ERROR: CO2 Modbus read registers failed for ID 1: %s\n", modbus_strerror(errno));
                log_error("CO2 Modbus read registers failed for ID 1: %s", modbus_strerror(errno));
                modbus_close(ctx);
                ctx_connected = 0;
                for (int retry = 0; retry < MAX_CONNECT_RETRIES; retry++) {
                    if (modbus_connect(ctx) == 0) {
                        ctx_connected = 1;
                        printf("INFO: CO2 Modbus reconnected for ID 1\n");
                        log_data("CO2 Modbus reconnected for ID 1");
                        break;
                    }
                    printf("ERROR: CO2 Modbus reconnect failed for ID 1 (attempt %d/%d): %s\n",
                           retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                    log_error("CO2 Modbus reconnect failed for ID 1 (attempt %d/%d): %s",
                              retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                    sleep(RETRY_DELAY_SEC);
                }
                if (!ctx_connected) {
                    printf("ERROR: Failed to reconnect to CO2 ID 1 after %d retries\n", MAX_CONNECT_RETRIES);
                    log_error("Failed to reconnect to CO2 ID 1 after %d retries", MAX_CONNECT_RETRIES);
                }
                if (modbus_write_registers(server_ctx, CO2_REGISTER_START, NUM_OF_REGISTERS, write_regs) == -1) {
                    printf("ERROR: Failed to write zeroed registers to server for ID 1: %s\n", modbus_strerror(errno));
                    log_error("Failed to write zeroed registers to server for ID 1: %s", modbus_strerror(errno));
                } else {
                    printf("INFO: Wrote zeroed registers to server for ID 1 at register %d\n", CO2_REGISTER_START);
                    log_data("Wrote zeroed registers to server for ID 1 at register %d", CO2_REGISTER_START);
                }
                sleep(10);
                continue;
            }

            printf("INFO: CO2 (ID 1): Read %d registers\n", num_read);
            log_data("CO2 (ID 1): Read %d registers", num_read);
            if (num_read >= 5) { // Ensure enough registers for scaling
                // Log raw register values for debugging
                for (int i = 0; i < num_read; i++) {
                    printf("INFO: CO2 (ID 1): reg[%d] = %u\n", i, reg[i]);
                    log_data("CO2 (ID 1): reg[%d] = %u", i, reg[i]);
                }

                // Scale sensor data
                float raw = reg[0];
                float ppm = (raw / 4095.0) * 2000; // CO2 (ppm)
                write_regs[0] = (uint16_t)ppm;
                float raw1 = reg[1];
                float ppm1 = (raw1 / 4095.0) * 5000; // Temperature (°C)
                write_regs[1] = (uint16_t)ppm1;
                float raw2 = reg[2];
                float ppm2 = (raw2 / 4095.0) * 100; // Humidity (%)
                write_regs[2] = (uint16_t)ppm2;
                float raw3 = reg[3];
                float ppm3 = (raw3 / 4095.0) * 600; // Pressure
                write_regs[3] = (uint16_t)ppm3;
                float raw4 = reg[4];
                float ppm4 = (raw4 / 4095.0) * 500; // VOC
                write_regs[4] = (uint16_t)ppm4;

                // Copy remaining registers
                for (int i = 5; i < NUM_OF_REGISTERS && i < num_read; i++) {
                    write_regs[i] = reg[i];
                }

                // Write to server
                if (modbus_write_registers(server_ctx, CO2_REGISTER_START, NUM_OF_REGISTERS, write_regs) == -1) {
                    printf("ERROR: Failed to write to server registers for ID 1: %s\n", modbus_strerror(errno));
                    log_error("Failed to write to server registers for ID 1: %s", modbus_strerror(errno));
                    modbus_close(server_ctx);
                    server_ctx_connected = 0;
                    for (int retry = 0; retry < MAX_CONNECT_RETRIES; retry++) {
                        if (modbus_connect(server_ctx) == 0) {
                            server_ctx_connected = 1;
                            printf("INFO: Server Modbus reconnected\n");
                            log_data("Server Modbus reconnected");
                            break;
                        }
                        printf("ERROR: Server Modbus reconnect failed (attempt %d/%d): %s\n",
                               retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                        log_error("Server Modbus reconnect failed (attempt %d/%d): %s",
                                  retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                        sleep(RETRY_DELAY_SEC);
                    }
                    if (!server_ctx_connected) {
                        printf("ERROR: Failed to reconnect to server after %d retries\n", MAX_CONNECT_RETRIES);
                        log_error("Failed to reconnect to server after %d retries", MAX_CONNECT_RETRIES);
                        sleep(10);
                        continue;
                    }
                    sleep(10);
                    continue;
                }

                // Log and print data
                printf("INFO: CO2 (ID 1): raw0 = %d, ppm0 = %.2f, scaled_ppm0 = %d, write to server register[%d]\n", 
                       (int)raw, ppm, write_regs[0], CO2_REGISTER_START);
                log_data("CO2 (ID 1): raw0 = %d, ppm0 = %.2f, scaled_ppm0 = %d, write to server register[%d]", 
                         (int)raw, ppm, write_regs[0], CO2_REGISTER_START);
                printf("INFO: CO2 (ID 1): raw1 = %d, ppm1 = %.2f, scaled_ppm1 = %d\n", (int)raw1, ppm1, write_regs[1]);
                log_data("CO2 (ID 1): raw1 = %d, ppm1 = %.2f, scaled_ppm1 = %d", (int)raw1, ppm1, write_regs[1]);
                printf("INFO: CO2 (ID 1): raw2 = %d, ppm2 = %.2f, scaled_ppm2 = %d\n", (int)raw2, ppm2, write_regs[2]);
                log_data("CO2 (ID 1): raw2 = %d, ppm2 = %.2f, scaled_ppm2 = %d", (int)raw2, ppm2, write_regs[2]);
                printf("INFO: CO2 (ID 1): raw3 = %d, ppm3 = %.2f, scaled_ppm3 = %d\n", (int)raw3, ppm3, write_regs[3]);
                log_data("CO2 (ID 1): raw3 = %d, ppm3 = %.2f, scaled_ppm3 = %d", (int)raw3, ppm3, write_regs[3]);
                printf("INFO: CO2 (ID 1): raw4 = %d, ppm4 = %.2f, scaled_ppm4 = %d\n", (int)raw4, ppm4, write_regs[4]);
                log_data("CO2 (ID 1): raw4 = %d, ppm4 = %.2f, scaled_ppm4 = %d", (int)raw4, ppm4, write_regs[4]);
                for (int i = 5; i < num_read && i < NUM_OF_REGISTERS; i++) {
                    printf("INFO: CO2 (ID 1): reg[%d] = %d\n", i, reg[i]);
                    log_data("CO2 (ID 1): reg[%d] = %d", i, reg[i]);
                }
            } else {
                printf("ERROR: Insufficient registers read for ID 1: %d\n", num_read);
                log_error("Insufficient registers read for ID 1: %d", num_read);
            }
        } else {
            printf("INFO: Skipping CO2 ID 1 due to connection failure\n");
            log_data("Skipping CO2 ID 1 due to connection failure");
        }

        // Read and process second sensor (ID 2)
        if (ctx1_connected) {
            memset(write_regs1, 0, sizeof(write_regs1)); // Initialize to zero
            int num_read = modbus_read_registers(ctx1, 0, NUM_OF_REGISTERS, reg1);
            if (num_read == -1) {
                printf("ERROR: CO2 Modbus read registers failed for ID 2: %s\n", modbus_strerror(errno));
                log_error("CO2 Modbus read registers failed for ID 2: %s", modbus_strerror(errno));
                modbus_close(ctx1);
                ctx1_connected = 0;
                for (int retry = 0; retry < MAX_CONNECT_RETRIES; retry++) {
                    if (modbus_connect(ctx1) == 0) {
                        ctx1_connected = 1;
                        printf("INFO: CO2 Modbus reconnected for ID 2\n");
                        log_data("CO2 Modbus reconnected for ID 2");
                        break;
                    }
                    printf("ERROR: CO2 Modbus reconnect failed for ID 2 (attempt %d/%d): %s\n",
                           retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                    log_error("CO2 Modbus reconnect failed for ID 2 (attempt %d/%d): %s",
                              retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                    sleep(RETRY_DELAY_SEC);
                }
                if (!ctx1_connected) {
                    printf("ERROR: Failed to reconnect to CO2 ID 2 after %d retries\n", MAX_CONNECT_RETRIES);
                    log_error("Failed to reconnect to CO2 ID 2 after %d retries", MAX_CONNECT_RETRIES);
                }
                if (modbus_write_registers(server_ctx, CO2_REGISTER_START2, NUM_OF_REGISTERS, write_regs1) == -1) {
                    printf("ERROR: Failed to write zeroed registers to server for ID 2: %s\n", modbus_strerror(errno));
                    log_error("Failed to write zeroed registers to server for ID 2: %s", modbus_strerror(errno));
                } else {
                    printf("INFO: Wrote zeroed registers to server for ID 2 at register %d\n", CO2_REGISTER_START2);
                    log_data("Wrote zeroed registers to server for ID 2 at register %d", CO2_REGISTER_START2);
                }
                sleep(10);
                continue;
            }

            printf("INFO: CO2 (ID 2): Read %d registers\n", num_read);
            log_data("CO2 (ID 2): Read %d registers", num_read);
            if (num_read >= 5) { // Ensure enough registers for scaling
                // Log raw register values for debugging
                for (int i = 0; i < num_read; i++) {
                    printf("INFO: CO2 (ID 2): reg1[%d] = %u\n", i, reg1[i]);
                    log_data("CO2 (ID 2): reg1[%d] = %u", i, reg1[i]);
                }

                // Scale sensor data
                float raw6 = reg1[0];
                float ppm6 = (raw6 / 4095.0) * 2000; // CO2 (ppm)
                write_regs1[0] = (uint16_t)ppm6;
                float raw7 = reg1[1];
                float ppm7 = (raw7 / 4095.0) * 5000; // Temperature (°C)
                write_regs1[1] = (uint16_t)ppm7;
                float raw8 = reg1[2];
                float ppm8 = (raw8 / 4095.0) * 100; // Humidity (%)
                write_regs1[2] = (uint16_t)ppm8;
                float raw9 = reg1[3];
                float ppm9 = (raw9 / 4095.0) * 600; // Pressure
                write_regs1[3] = (uint16_t)ppm9;
                float raw10 = reg1[4];
                float ppm10 = (raw10 / 4095.0) * 500; // VOC
                write_regs1[4] = (uint16_t)ppm10;

                // Copy remaining registers
                for (int i = 5; i < NUM_OF_REGISTERS && i < num_read; i++) {
                    write_regs1[i] = reg1[i];
                }

                // Write to server
                if (modbus_write_registers(server_ctx, CO2_REGISTER_START2, NUM_OF_REGISTERS, write_regs1) == -1) {
                    printf("ERROR: Failed to write to server registers for ID 2: %s\n", modbus_strerror(errno));
                    log_error("Failed to write to server registers for ID 2: %s", modbus_strerror(errno));
                    modbus_close(server_ctx);
                    server_ctx_connected = 0;
                    for (int retry = 0; retry < MAX_CONNECT_RETRIES; retry++) {
                        if (modbus_connect(server_ctx) == 0) {
                            server_ctx_connected = 1;
                            printf("INFO: Server Modbus reconnected\n");
                            log_data("Server Modbus reconnected");
                            break;
                        }
                        printf("ERROR: Server Modbus reconnect failed (attempt %d/%d): %s\n",
                               retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                        log_error("Server Modbus reconnect failed (attempt %d/%d): %s",
                                  retry + 1, MAX_CONNECT_RETRIES, modbus_strerror(errno));
                        sleep(RETRY_DELAY_SEC);
                    }
                    if (!server_ctx_connected) {
                        printf("ERROR: Failed to reconnect to server after %d retries\n", MAX_CONNECT_RETRIES);
                        log_error("Failed to reconnect to server after %d retries", MAX_CONNECT_RETRIES);
                        sleep(10);
                        continue;
                    }
                    sleep(10);
                    continue;
                }

                // Log and print data
                printf("INFO: CO2 (ID 2): raw6 = %d, ppm6 = %.2f, scaled_ppm6 = %d, write to server register[%d]\n", 
                       (int)raw6, ppm6, write_regs1[0], CO2_REGISTER_START2);
                log_data("CO2 (ID 2): raw6 = %d, ppm6 = %.2f, scaled_ppm6 = %d, write to server register[%d]", 
                         (int)raw6, ppm6, write_regs1[0], CO2_REGISTER_START2);
                printf("INFO: CO2 (ID 2): raw7 = %d, ppm7 = %.2f, scaled_ppm7 = %d\n", (int)raw7, ppm7, write_regs1[1]);
                log_data("CO2 (ID 2): raw7 = %d, ppm7 = %.2f, scaled_ppm7 = %d", (int)raw7, ppm7, write_regs1[1]);
                printf("INFO: CO2 (ID 2): raw8 = %d, ppm8 = %.2f, scaled_ppm8 = %d\n", (int)raw8, ppm8, write_regs1[2]);
                log_data("CO2 (ID 2): raw8 = %d, ppm8 = %.2f, scaled_ppm8 = %d", (int)raw8, ppm8, write_regs1[2]);
                printf("INFO: CO2 (ID 2): raw9 = %d, ppm9 = %.2f, scaled_ppm9 = %d\n", (int)raw9, ppm9, write_regs1[3]);
                log_data("CO2 (ID 2): raw9 = %d, ppm9 = %.2f, scaled_ppm9 = %d", (int)raw9, ppm9, write_regs1[3]);
                printf("INFO: CO2 (ID 2): raw10 = %d, ppm10 = %.2f, scaled_ppm10 = %d\n", (int)raw10, ppm10, write_regs1[4]);
                log_data("CO2 (ID 2): raw10 = %d, ppm10 = %.2f, scaled_ppm10 = %d", (int)raw10, ppm10, write_regs1[4]);
                for (int i = 5; i < num_read && i < NUM_OF_REGISTERS; i++) {
                    printf("INFO: CO2 (ID 2): reg1[%d] = %d\n", i, reg1[i]);
                    log_data("CO2 (ID 2): reg1[%d] = %d", i, reg1[i]);
                }
                printf("\n\n");
            } else {
                printf("ERROR: Insufficient registers read for ID 2: %d\n", num_read);
                log_error("Insufficient registers read for ID 2: %d", num_read);
            }
        } else {
            printf("INFO: Skipping CO2 ID 2 due to connection failure\n");
            log_data("Skipping CO2 ID 2 due to connection failure");
        }

        sleep(10); // Delay between iterations
    }

    // Cleanup (unreachable due to while(1), but included for completeness)
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
    }
    if (ctx1) {
        modbus_close(ctx1);
        modbus_free(ctx1);
    }
    if (server_ctx) {
        modbus_close(server_ctx);
        modbus_free(server_ctx);
    }
    printf("INFO: CO2 Client Thread Exit\n");
    log_data("CO2 Client Thread Exit");
    return NULL;
}
