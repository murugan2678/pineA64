#include "co2_client.h"
#include "config.h"
#include "server.h"

static int keep_running = 1;

void *co2_client_thread(void *arg)
{
    modbus_t *ctx[MAX_DEVICES] = {NULL};
    uint16_t reg[5], reg1[5];
    uint16_t scaled_ppm_reg[5], scaled_ppm_reg1[5];
    float ppm;
    time_t last_log_time = time(NULL);

    // Initialize Modbus contexts for devices
    for (int i = 0; i < total_devices; i++)
    {
        ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);
        if (ctx[i] == NULL)
        {
            log_error("Failed to create Modbus context for device %s", devices[i].device_IP);
            printf("Failed to create Modbus context for device %s\n", devices[i].device_IP);
            continue;
        }

        if (modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0)
        {
            log_error("Failed to set slave ID %d for device %s: %s", 
                      devices[i].device_slave_ID, devices[i].device_IP, modbus_strerror(errno));
            printf("Failed to set slave ID %d for device %s: %s\n", 
                   devices[i].device_slave_ID, devices[i].device_IP, modbus_strerror(errno));
            modbus_free(ctx[i]);
            ctx[i] = NULL;
            continue;
        }

        if (modbus_connect(ctx[i]) != 0)
        {
            log_error("Failed to connect to device %s: %s", devices[i].device_IP, modbus_strerror(errno));
            printf("Failed to connect to device %s: %s\n", devices[i].device_IP, modbus_strerror(errno));
            modbus_free(ctx[i]);
            ctx[i] = NULL;
            continue;
        }

        // Set timeout for client context (200ms)
        uint32_t old_response_to_sec, old_response_to_usec;
        modbus_get_response_timeout(ctx[i], &old_response_to_sec, &old_response_to_usec);
        modbus_set_response_timeout(ctx[i], 0, 200000); // 200ms

        printf("Connected to client %s device %s\n", (i == 0) ? "first" : "second", devices[i].device_IP);
    }

    // Initialize server context
    modbus_t *server_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (server_ctx == NULL || modbus_connect(server_ctx) != 0)
    {
        log_error("Failed to connect to Modbus server %s:%d: %s", SERVER_ADDRESS, SERVER_PORT, modbus_strerror(errno));
        printf("Failed to connect to Modbus server %s:%d: %s\n", SERVER_ADDRESS, SERVER_PORT, modbus_strerror(errno));
        for (int i = 0; i < total_devices; i++)
        {
            if (ctx[i]) modbus_free(ctx[i]);
        }
        return NULL;
    }

    // Set timeout for server context (optional, 200ms)
    uint32_t server_old_to_sec, server_old_to_usec;
    modbus_get_response_timeout(server_ctx, &server_old_to_sec, &server_old_to_usec);
    modbus_set_response_timeout(server_ctx, 0, 200000); // 200ms

    if (modbus_set_slave(server_ctx, SERVER_SLAVE_ID) == -1)
    {
        log_error("Failed to set slave ID %d for server: %s", SERVER_SLAVE_ID, modbus_strerror(errno));
        printf("Failed to set slave ID %d for server: %s\n", SERVER_SLAVE_ID, modbus_strerror(errno));
        modbus_free(server_ctx);
        for (int i = 0; i < total_devices; i++)
        {
            if (ctx[i]) modbus_free(ctx[i]);
        }
        return NULL;
    }

    while (keep_running)
    {
        time_t current_time = time(NULL);
        if (current_time - last_log_time >= 30)
        {
            log_data("CO2 client thread is active. Current time: %s", ctime(&current_time));
            printf("CO2 client thread is active at %s", ctime(&current_time));
            last_log_time = current_time;
        }

        for (int i = 0; i < total_devices; i++)
        {
            uint16_t *current_reg = (i == 0) ? reg : reg1;
            uint16_t *current_scaled_ppm = (i == 0) ? scaled_ppm_reg : scaled_ppm_reg1;
            int start_reg = (i == 0) ? CO2_REGISTER_START : CO2_REGISTER_START2;
            int num_regs = NUM_OF_REGISTERS;

            // Reconnect if context is NULL
            if (ctx[i] == NULL)
            {
                ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);
                if (ctx[i] == NULL || modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0 || modbus_connect(ctx[i]) != 0)
                {
                    log_error("Failed to reconnect to device %s: %s", devices[i].device_IP, modbus_strerror(errno));
                    printf("Failed to reconnect to device %s: %s\n", devices[i].device_IP, modbus_strerror(errno));
                    ctx[i] = NULL;

                    // Write zeros to server registers for failed device
                    uint16_t zero_regs[5] = {0};
                    if (modbus_write_registers(server_ctx, start_reg, 5, zero_regs) == -1)
                    {
                        log_error("Failed to write zeros to registers %d-%d: %s", 
                                  start_reg, start_reg + 4, modbus_strerror(errno));
                        printf("Failed to write zeros to registers %d-%d: %s\n", 
                               start_reg, start_reg + 4, modbus_strerror(errno));
                    }
                    else
                    {
                        printf("Wrote zeros to registers %d-%d due to device %d failure\n", 
                               start_reg, start_reg + 4, devices[i].id);
                    }
                    continue;
                }

                // Set timeout for reconnected client context (200ms)
                modbus_get_response_timeout(ctx[i], &old_response_to_sec, &old_response_to_usec);
                modbus_set_response_timeout(ctx[i], 0, 200000); // 200ms

                printf("Reconnected to device %s\n", devices[i].device_IP);
            }

            // Rest of the read/write loop remains the same...
            memset(current_reg, 0, sizeof(uint16_t) * num_regs);
            memset(current_scaled_ppm, 0, sizeof(uint16_t) * num_regs);
            int success = 0;
            int retry_count = 0;
            const int max_retries = 5;

            while (retry_count < max_retries && keep_running)
            {
                int rc = modbus_read_registers(ctx[i], 0, num_regs, current_reg);
                if (rc == num_regs)
                {
                    success = 1;
                    break;
                }
                else
                {
                    log_error("CO2 Modbus read failed for device %s: %s", devices[i].device_IP, modbus_strerror(errno));
                    printf("CO2 Modbus read failed for device %s: %s (rc: %d)\n", 
                           devices[i].device_IP, modbus_strerror(errno), rc);
                    modbus_close(ctx[i]);
                    ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);
                    if (ctx[i] == NULL || modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0 || modbus_connect(ctx[i]) != 0)
                    {
                        log_error("CO2 Modbus reconnect failed for device %s: %s", 
                                  devices[i].device_IP, modbus_strerror(errno));
                        printf("CO2 Modbus reconnect failed for device %s: %s\n", 
                               devices[i].device_IP, modbus_strerror(errno));
                        ctx[i] = NULL;
                        break;
                    }
                    // Set timeout for reconnected client context (200ms)
                    modbus_get_response_timeout(ctx[i], &old_response_to_sec, &old_response_to_usec);
                    modbus_set_response_timeout(ctx[i], 0, 200000); // 200ms
                    retry_count++;
                    sleep(3);
                }
            }

            // Handle read failure
            if (!success && keep_running)
            {
                log_error("Failed to read registers for device %s", devices[i].device_IP);
                printf("Failed to read registers for device %s\n", devices[i].device_IP);
                uint16_t zero_regs[5] = {0};
                if (modbus_write_registers(server_ctx, start_reg, 5, zero_regs) == -1)
                {
                    log_error("Failed to write zeros to registers %d-%d: %s", 
                              start_reg, start_reg + 4, modbus_strerror(errno));
                    printf("Failed to write zeros to registers %d-%d: %s\n", 
                           start_reg, start_reg + 4, modbus_strerror(errno));
                }
                else
                {
                    printf("Wrote zeros to registers %d-%d due to device %d failure\n", 
                           start_reg, start_reg + 4, devices[i].id);
                }
            }

            // Process and write successful reads
            if (success)
            {
                for (int j = 0; j < num_regs; j++)
                {
                    float scale = (j == 0 ? 2000 : j == 1 ? 5000 : j == 2 ? 100 : j == 3 ? 600 : 500);
                    ppm = (float)current_reg[j] / 4095.0 * scale;
                    current_scaled_ppm[j] = (int)ppm;
                    printf("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", 
                           j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);
                    log_data("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", 
                             j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);
                }

                // Write scaled_ppm to server
                printf("Writing scaled_ppm to registers %d-%d: [%d, %d, %d, %d, %d]\n", 
                       start_reg, start_reg + num_regs - 1, 
                       current_scaled_ppm[0], current_scaled_ppm[1], current_scaled_ppm[2], 
                       current_scaled_ppm[3], current_scaled_ppm[4]);
                if (modbus_write_registers(server_ctx, start_reg, num_regs, current_scaled_ppm) == -1)
                {
                    log_error("Failed to write to server registers %d-%d: %s", 
                              start_reg, start_reg + num_regs - 1, modbus_strerror(errno));
                    printf("Failed to write to server registers %d-%d: %s\n", 
                           start_reg, start_reg + num_regs - 1, modbus_strerror(errno));
                    modbus_close(server_ctx);
                    if (modbus_connect(server_ctx) == -1)
                    {
                        log_error("Server Modbus reconnect failed: %s", modbus_strerror(errno));
                        printf("Server Modbus reconnect failed: %s\n", modbus_strerror(errno));
                    }
                }
            }
        }
        sleep(10);
    }

    // Cleanup
    if (server_ctx != NULL)
    {
        uint16_t zero_regs[20] = {0};
        if (modbus_write_registers(server_ctx, 0, 20, zero_regs) == -1)
        {
            log_error("Failed to write zeros to server registers on exit: %s", modbus_strerror(errno));
            printf("Failed to write zeros to server registers on exit: %s\n", modbus_strerror(errno));
        }
        else
        {
            printf("Wrote zeros to server registers on CO2 client exit\n");
        }
        modbus_close(server_ctx);
        modbus_free(server_ctx);
    }

    for (int i = 0; i < total_devices; i++)
    {
        if (ctx[i])
        {
            modbus_close(ctx[i]);
            modbus_free(ctx[i]);
        }
    }

    printf("CO2 client thread exited\n");
    fflush(stdout);
    return NULL;
}
