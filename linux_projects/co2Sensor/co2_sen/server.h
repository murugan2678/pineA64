#ifndef SERVER_H
#define SERVER_H

#include "config.h"

// Pine board server initalising here
#define SERVER_PORT 5503 // pine port
#define SERVER_ADDRESS "192.168.0.136" // this for Pine IP Address, this ip set modbus poll software read data for modbus poll software 
#define SERVER_SLAVE_ID 1 // === change for server slave set 1
#define NB_CONNECTION 5

void *server_thread(void *arg);

#endif
