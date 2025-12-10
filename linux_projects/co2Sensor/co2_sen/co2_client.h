#ifndef CO2_CLIENT_H
#define CO2_CLIENT_H

#include "config.h"

#define NUM_OF_REGISTERS 5
//#define NUM_OF_REGISTERS1 8
#define CO2_REGISTER_START 0 // Store CO2 data at Modbus register 0
#define CO2_REGISTER_START2 10

void *co2_client_thread(void *arg);

#endif
