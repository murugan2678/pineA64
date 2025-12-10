#include "jsonify.h"
#include "boodskap.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define B_CUSTOMER_ID "ZBCID00012"
#define B_TENANT_ID "ZBTID0001"
#define B_DEVICE_ID "ZBDID02BA2CA259E4"
#define BUFFER_SIZE 2048

void createTestPkt(char *buffer)
{
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "name", "John Doe");
  cJSON_AddNumberToObject(json, "age", 30);
  cJSON_AddStringToObject(json, "email", "john.doe@example.com");

  char *json_str = cJSON_PrintUnformatted(json);

  strncpy(buffer, json_str, BUFFER_SIZE - 1);
  buffer[BUFFER_SIZE -1] = '\0';

  free(json_str);
  cJSON_Delete(json);
}

void createMainPkt_B(char *buffer, int seq, time_t current_time, sensorData *sensor_data, int sensor_count)
{
  cJSON *json = cJSON_CreateObject();
  char message[25];  

  sprintf(message, "%ld000", current_time);

  //  cjson formate here
  cJSON_AddStringToObject(json, "timestamp", message);
  cJSON_AddStringToObject(json, "customerid", B_CUSTOMER_ID);
  cJSON_AddStringToObject(json, "tenantid", B_TENANT_ID);
  cJSON_AddStringToObject(json, "deviceid", B_DEVICE_ID);
  cJSON_AddStringToObject(json, "datatype", "metadata");
  cJSON_AddStringToObject(json, "datamode", "monitoring");

  // cjson array create inside data
  cJSON *data_array = cJSON_CreateArray();

  // equipment id
  cJSON_AddItemToObject(json, "equipment", data_array);

  for (int i = 0; i < sensor_count; i++)
  {
    int sensor_index = (i < 2) ? 0 : 1;
    // this function call B_sensor
    B_sensor(data_array, i, &sensor_data[sensor_index], current_time, NULL);
  }

  cJSON_PrintPreallocated(json, buffer, BUFFER_SIZE, 0);
  cJSON_Delete(json);
}
