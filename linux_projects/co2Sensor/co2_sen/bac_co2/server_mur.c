#include "server.h"
#include "config.h"
#include <modbus/modbus.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

static modbus_t *sctx = NULL;
static int server_socket = -1;
modbus_mapping_t *mb_mapping = NULL;

static void close_sigint(int dummy)
{
    if (server_socket != -1)
    {
        close(server_socket);
    }
    if (sctx)
    {
        modbus_free(sctx);
    }
    if (mb_mapping)
    {
        modbus_mapping_free(mb_mapping);
    }
    exit(0);
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
    if (mb_mapping == NULL)
    {
        log_error("Failed to allocate Modbus mapping: %s", modbus_strerror(errno));
        return NULL;
    }

    // Initialize Modbus TCP server
    sctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (sctx == NULL)
    {
        log_error("Failed to create Modbus TCP context: %s", modbus_strerror(errno));
        modbus_mapping_free(mb_mapping);
        return NULL;
    }

    // Set slave ID
    if (modbus_set_slave(sctx, SERVER_SLAVE_ID) == -1)
    {
        log_error("Failed to set slave ID: %s", modbus_strerror(errno));
        modbus_free(sctx);
        modbus_mapping_free(mb_mapping);
        return NULL;
    }

    server_socket = modbus_tcp_listen(sctx, NB_CONNECTION);
    if (server_socket == -1)
    {
        log_error("Unable to listen TCP connection: %s", modbus_strerror(errno));
        modbus_free(sctx);
        modbus_mapping_free(mb_mapping);
        return NULL;
    }

    signal(SIGINT, close_sigint);

    FD_ZERO(&refset);
    FD_SET(server_socket, &refset);
    fdmax = server_socket;
    header_length = modbus_get_header_length(sctx);

    log_data("Modbus server started on %s:%d, Slave ID: %d", SERVER_ADDRESS, SERVER_PORT, SERVER_SLAVE_ID);

    while (1)
    {
        time_t current_time = time(NULL);
        if (current_time - last_log_time >= 30)
        {
            log_data("Server thread is active. Current time: %s", ctime(&current_time));
            last_log_time = current_time;
        }

        rdset = refset;
        struct timeval timeout = {5, 0}; // 5-second timeout for select
        if (select(fdmax + 1, &rdset, NULL, NULL, &timeout) == -1)
        {
            log_error("Server select() failure: %s", strerror(errno));
            sleep(5);
            continue;
        }

        for (master_socket = 0; master_socket <= fdmax; master_socket++)
        {
            if (!FD_ISSET(master_socket, &rdset))
            {
                continue;
            }

            if (master_socket == server_socket)
            {
                struct sockaddr_in clientaddr;
                socklen_t addrlen = sizeof(clientaddr);
                int newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1)
                {
                    log_error("Server accept() error: %s", strerror(errno));
                }
                else
                {
                    FD_SET(newfd, &refset);
                    if (newfd > fdmax)
                    {
                        fdmax = newfd;
                    }
                    log_data("New connection from %s:%d on socket %d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                }
            }
            else
            {
                modbus_set_socket(sctx, master_socket);
                rc = modbus_receive(sctx, query);
                if (rc > 0)
                {
                    if (query[header_length] == 0x03 || query[header_length] == 0x10) // Read Holding or Write Multiple
                    {
                        // Validate register bounds for write requests
                        if (query[header_length] == 0x10)
                        {
                            int start_addr = (query[header_length + 1] << 8) | query[header_length + 2];
                            int nb = (query[header_length + 3] << 8) | query[header_length + 4];
                            if (start_addr + nb > mb_mapping->nb_registers)
                            {
                                log_error("Write request exceeds register bounds: start=%d, nb=%d, max=%d", start_addr, nb, mb_mapping->nb_registers);
                                modbus_reply_exception(sctx, query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
                                continue;
                            }
                        }

                        rc = modbus_reply(sctx, query, rc, mb_mapping);
                        if (rc == -1)
                        {
                            log_error("Modbus reply failed: %s", modbus_strerror(errno));
                        }
                        else if (query[header_length] == 0x10)
                        {
                            int start_addr = (query[header_length + 1] << 8) | query[header_length + 2];
                            int nb = (query[header_length + 3] << 8) | query[header_length + 4];
                            log_data("Wrote %d registers starting at %d:", nb, start_addr);
                            for (int i = 0; i < nb; i++)
                            {
                                log_data("Register[%d] = %d", start_addr + i, mb_mapping->tab_registers[start_addr + i]);
                            }
                        }
                    }
                    else
                    {
                        log_data("Unsupported Modbus function code: %d", query[header_length]);
                        modbus_reply_exception(sctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
                    }
                }
                else if (rc == -1)
                {
                    log_data("Connection closed on socket %d", master_socket);
                    close(master_socket);
                    FD_CLR(master_socket, &refset);
                    // Recalculate fdmax
                    fdmax = server_socket;
                    for (int i = 0; i <= FD_SETSIZE; i++)
                    {
                        if (FD_ISSET(i, &refset) && i > fdmax)
                        {
                            fdmax = i;
                        }
                    }
                }
            }
        }
    }

    log_data("Server Thread Exit");
    close_sigint(0);
    return NULL;
}
