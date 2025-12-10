#ifndef _BOODSKAP_H
#define _BOODSKAP_H

#include "data.h"
#include "cjson/cJSON.h"
#include <time.h>
#include <stdio.h>


/* extern uint16_t sen_co2_id[];
extern uint16_t sen2_co2_id[]; */


void B_sensor(cJSON *root, uint8_t id, sensorData *co2_sen, time_t timestamp, char *id_str);

#endif
