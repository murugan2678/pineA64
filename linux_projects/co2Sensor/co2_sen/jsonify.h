#ifndef _JSONIFY_H
#define _JSONIFY_H

#include "cjson/cJSON.h"
#include "data.h"
#include <time.h>

void createTestPkt(char *buffer);
void createMainPkt_B(char *buffer, int seq, time_t current_time, sensorData *sensor_data, int sensor_count);

#endif
