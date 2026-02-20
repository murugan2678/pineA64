#ifndef JSONIFY_H
#define JSONIFY_H

#include <time.h>
#include <stdint.h>
#include "data.h"  // vav register header for data.h

#define BUFFER_SIZE 2048   // buf size create for 2048

// json default formate create function
void creatTestPkt(char *buffer);	 

// json create function
void createVavJson(char *buffer, const VavSensorData *vav_data, int sensor_count, time_t current_time);

#endif
