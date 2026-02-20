#ifndef _BOODSKAP_H
#define _BOODSKAP_H

#include  "data.h"
#include "cjson/cJSON.h"
#include <time.h>
#include <stdio.h>
#include <unistd.h>


//  boodskap send the mqtt data function
void B_sensor(cJSON *root_array, uint8_t sensor_idx, const VavSensorData *vav, time_t timestamp, const char *id_str);

#endif
