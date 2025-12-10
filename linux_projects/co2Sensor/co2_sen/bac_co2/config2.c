#include "co2_client.h"
#include "config.h"
#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void *co2_client_thread(void *arg)
{
    modbus_t *ctx[MAX_DEVICES] = {NULL};
    uint16_t reg[5], reg1[5]; // Read 5 registers as per output
    float ppm;
    int scaled_ppm;
    time_t last_log_time = time(NULL);

    // Initialize Modbus contexts
    for (int i = 0; i < total_devices; i++) {
        ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);
        if (ctx[i] == NULL) {
            log_error("Failed to create Modbus context for device %d (%s:%d)", devices[i].id, devices[i].device_IP, devices[i].device_port_number);
            continue;
        }
        if (modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0) {
            log_error("Failed to set slave ID %d for device %d", devices[i].device_slave_ID, devices[i].id);
            modbus_free(ctx[i]);
            ctx[i] = NULL;
            continue;
        }
        if (modbus_connect(ctx[i]) != 0) {
            log_error("Failed to connect to device %d (%s:%d): %s", devices[i].id, devices[i].device_IP, devices[i].device_port_number, modbus_strerror(errno));
            modbus_free(ctx[i]);
            ctx[i] = NULL;
        }
    }

    modbus_t *server_ctx = modbus_new_tcp("192.168.0.136", 5503);
    if (server_ctx == NULL || modbus_connect(server_ctx) != 0) {
        log_error("Failed to connect to Modbus server: %s", modbus_strerror(errno));
        for (int i = 0; i < total_devices; i++) {
            if (ctx[i]) modbus_free(ctx[i]);
        }
        return NULL;
    }

    while (1) {
        time_t current_time = time(NULL);
        for (int i = 0; i < total_devices; i++) {
            uint16_t *current_reg = (i == 0) ? reg : reg1;
            int start_reg = (i == 0) ? 0 : 10; // Sensor 1: reg 0, Sensor 2: reg 10
            int num_regs = 5; // Read 5 registers as per output

            // Initialize registers to 0
            memset(current_reg, 0, sizeof(uint16_t) * num_regs);

            // Read registers (only CO2 for MQTT, but read all 5 for printing)
            if (ctx[i] != NULL) {
                int rc = modbus_read_registers(ctx[i], 0, num_regs, current_reg);
                if (rc == -1) {
                    log_error("CO2 Modbus read registers failed index %d : %s", devices[i].id, modbus_strerror(errno));
                    modbus_close(ctx[i]);
                    modbus_free(ctx[i]);
                    ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);
                    if (ctx[i] == NULL || modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0 || modbus_connect(ctx[i]) != 0) {
                        log_error("CO2 Modbus reconnect failed for device %d: %s", devices[i].id, modbus_strerror(errno));
                        modbus_free(ctx[i]);
                        ctx[i] = NULL;
                    }
                }
            } else {
                log_error("Skipping device %d due to invalid context", devices[i].id);
            }

            // Print registers (mimic output format)
            for (int j = 0; j < num_regs; j++) {
                printf("write reg%s %d : %d\n", (i == 0) ? "" : "1", j, current_reg[j]);
            }
            // Print CO2 conversions
            for (int j = 0; j < num_regs; j++) {
                float scale = (j == 0 ? 2000 : j == 1 ? 5000 : j == 2 ? 100 : j == 3 ? 600 : 500);
                ppm = (float)current_reg[j] / 4095.0 * scale;
                scaled_ppm = (int)ppm;
                printf("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", 
                       j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, scaled_ppm);
            }
            // Print extra registers
            for (int j = 5; j < 8; j++) {
                printf("CO2 : reg%s[%d] = %d\n", (i == 0) ? "" : "1", j, 0);
            }

            // Write only CO2 (first register) to server
            if (modbus_write_register(server_ctx, start_reg, current_reg[0]) == -1) {
                log_error("Failed to write to server register %d: %s", start_reg, modbus_strerror(errno));
                modbus_close(server_ctx);
                if (modbus_connect(server_ctx) == -1) {
                    log_error("Server Modbus reconnect failed: %s", modbus_strerror(errno));
                }
            }
        }

        // Log activity every 30 seconds
        if (current_time - last_log_time >= 30) {
            log_data("CO2 client thread is active. Current time: %s", ctime(&current_time));
            last_log_time = current_time;
        }
        sleep(10); // Read every 10 seconds
    }

    // Cleanup
    for (int i = 0; i < total_devices; i++) {
        if (ctx[i]) {
            modbus_close(ctx[i]);
            modbus_free(ctx[i]);
        }
    }
    modbus_close(server_ctx);
    modbus_free(server_ctx);
    return NULL;
}
