#include "server.h"
#include "config.h"
#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

static modbus_t *sctx = NULL;
static modbus_mapping_t *mb_mapping = NULL;
static int server_socket = -1;
static int keep_running = 1; // Control flag for thread termination

static void close_sigint(int dummy)
{
    keep_running = 0; // Signal main loop to exit
}

static void cleanup_modbus_server(void)
{
    if (mb_mapping != NULL) {
        // Clear registers
        for (int i = 0; i < mb_mapping->nb_registers; i++) {
            mb_mapping->tab_registers[i] = 0;
        }
        if (sctx != NULL) {
            uint16_t zero_regs[20] = {0};
            if (modbus_write_registers(sctx, 0, 20, zero_regs) == -1) {
                log_error("Failed to write zeros to registers: %s", modbus_strerror(errno));
                printf("Failed to write zeros to registers: %s\n", modbus_strerror(errno));
                fflush(stdout);
            } else {
                printf("Wrote zeros to Modbus registers\n");
                fflush(stdout);
            }
        }
        modbus_mapping_free(mb_mapping);
        mb_mapping = NULL;
        printf("Modbus mapping cleared and freed\n");
        fflush(stdout);
    }
    if (sctx != NULL) {
        modbus_close(sctx);
        modbus_free(sctx);
        sctx = NULL;
        printf("Modbus server closed\n");
        fflush(stdout);
    }
    if (server_socket != -1) {
        shutdown(server_socket, SHUT_RDWR);
        close(server_socket);
        server_socket = -1;
        printf("Modbus server socket closed\n");
        fflush(stdout);
    }
}

void *server_thread(void *arg)
{
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket;
    int rc;
    fd_set refset;
    fd_set rdset;
    int fdmax;
    int header_length;
    time_t last_log_time = time(NULL);

    // Initialize Modbus mapping
    mb_mapping = modbus_mapping_new(0, 0, MAX_REGISTERS, 0);
    if (mb_mapping == NULL) {
        log_error("Failed to allocate Modbus mapping: %s", modbus_strerror(errno));
        printf("Failed to allocate Modbus mapping: %s\n", modbus_strerror(errno));
        fflush(stdout);
        return NULL;
    }

    // Initialize Modbus TCP server
    sctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (sctx == NULL) {
        log_error("Failed to create Modbus TCP context: %s", modbus_strerror(errno));
        printf("Failed to create Modbus TCP context: %s\n", modbus_strerror(errno));
        fflush(stdout);
        modbus_mapping_free(mb_mapping);
        mb_mapping = NULL;
        return NULL;
    }

    // Set slave ID
    if (modbus_set_slave(sctx, SERVER_SLAVE_ID) == -1) {
        log_error("Failed to set slave ID %d: %s", SERVER_SLAVE_ID, modbus_strerror(errno));
        printf("Failed to set slave ID %d: %s\n", SERVER_SLAVE_ID, modbus_strerror(errno));
        fflush(stdout);
        modbus_free(sctx);
        sctx = NULL;
        modbus_mapping_free(mb_mapping);
        mb_mapping = NULL;
        return NULL;
    }

    // Create server socket and set SO_REUSEADDR
    server_socket = modbus_tcp_listen(sctx, NB_CONNECTION);
    if (server_socket == -1) {
        log_error("Unable to listen TCP connection: %s", modbus_strerror(errno));
        printf("Unable to listen TCP connection: %s\n", modbus_strerror(errno));
        fflush(stdout);
        modbus_free(sctx);
        sctx = NULL;
        modbus_mapping_free(mb_mapping);
        mb_mapping = NULL;
        return NULL;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        log_error("Failed to set SO_REUSEADDR: %s", strerror(errno));
        printf("Failed to set SO_REUSEADDR: %s\n", strerror(errno));
        fflush(stdout);
        modbus_close(sctx);
        modbus_free(sctx);
        sctx = NULL;
        modbus_mapping_free(mb_mapping);
        mb_mapping = NULL;
        close(server_socket);
        server_socket = -1;
        return NULL;
    }

    // Set up SIGINT handler
    signal(SIGINT, close_sigint);

    // Initialize file descriptor set
    FD_ZERO(&refset);
    FD_SET(server_socket, &refset);
    fdmax = server_socket;
    header_length = modbus_get_header_length(sctx);

    log_data("Modbus server started on %s:%d, Slave ID: %d", SERVER_ADDRESS, SERVER_PORT, SERVER_SLAVE_ID);
    printf("Modbus server started on %s:%d, Slave ID: %d\n", SERVER_ADDRESS, SERVER_PORT, SERVER_SLAVE_ID);
    fflush(stdout);

    while (keep_running) {
        time_t current_time = time(NULL);
        if (current_time - last_log_time >= 30) {
            log_data("Server thread is active. Current time: %s", ctime(&current_time));
            printf("Server thread is active at %s", ctime(&current_time));
            fflush(stdout);
            last_log_time = current_time;
        }

        rdset = refset;
        struct timeval timeout = {5, 0}; // 5-second timeout
        if (select(fdmax + 1, &rdset, NULL, NULL, &timeout) == -1) {
            if (errno == EINTR && !keep_running) {
                break; // Exit on SIGINT
            }
            log_error("Server select() failure: %s", strerror(errno));
            printf("Server select() failure: %s\n", strerror(errno));
            fflush(stdout);
            sleep(5);
            continue;
        }

        for (master_socket = 0; master_socket <= fdmax; master_socket++) {
            if (!FD_ISSET(master_socket, &rdset)) {
                continue;
            }

            if (master_socket == server_socket) {
                // New connection
                struct sockaddr_in clientaddr;
                socklen_t addrlen = sizeof(clientaddr);
                int newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1) {
                    if (keep_running) {
                        log_error("Server accept() error: %s", strerror(errno));
                        printf("Server accept() error: %s\n", strerror(errno));
                        fflush(stdout);
                    }
                } else {
                    FD_SET(newfd, &refset);
                    if (newfd > fdmax) {
                        fdmax = newfd;
                    }
                    log_data("New connection from %s:%d on socket %d", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), newfd);
                    printf("New connection from %s:%d on socket %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), newfd);
                    fflush(stdout);
                }
            } else {
                // Handle client request
                modbus_set_socket(sctx, master_socket);
                rc = modbus_receive(sctx, query);
                if (rc > 0) {
                    if (query[header_length] == 0x03 || query[header_length] == 0x10) { // Read Holding or Write Multiple
                        rc = modbus_reply(sctx, query, rc, mb_mapping);
                        if (rc == -1) {
                            log_error("Modbus reply failed: %s", modbus_strerror(errno));
                            printf("Modbus reply failed: %s\n", modbus_strerror(errno));
                            fflush(stdout);
                        } else if (query[header_length] == 0x10) {
                            int start_reg = (query[header_length + 1] << 8) | query[header_length + 2];
                            log_data("Wrote registers starting at %d", start_reg);
                            printf("Wrote registers starting at %d\n", start_reg);
                            fflush(stdout);
                        }
                    } else {
                        log_data("Unsupported Modbus function code: %d", query[header_length]);
                        printf("Unsupported Modbus function code: %d\n", query[header_length]);
                        fflush(stdout);
                        modbus_reply_exception(sctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
                    }
                } else if (rc == -1) {
                    log_data("Connection closed on socket %d", master_socket);
                    printf("Connection closed on socket %d\n", master_socket);
                    fflush(stdout);
                    close(master_socket);
                    FD_CLR(master_socket, &refset);
                    if (master_socket == fdmax) {
                        // Update fdmax
                        fdmax = server_socket;
                        for (int i = 0; i <= fdmax; i++) {
                            if (FD_ISSET(i, &refset) && i > fdmax) {
                                fdmax = i;
                            }
                        }
                    }
                }
            }
        }
    }

    // Cleanup on exit
    cleanup_modbus_server();
    log_data("Server Thread Exit");
    printf("Server Thread Exit\n");
    fflush(stdout);
    return NULL;
}
