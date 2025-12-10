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
  // cJSON * cJSON_CreateObject ()
  cJSON *json = cJSON_CreateObject();

  // cJSON_AddStringToObject(object, name, s) cJSON_AddItemToObject(object, name, cJSON_CreateString(s))
  cJSON_AddStringToObject(json, "name", "John Doe");

  // cJSON_AddNumberToObject(object, name, n) cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
  cJSON_AddNumberToObject(json, "age", 30);

  // cJSON_AddStringToObject(object, name, s) cJSON_AddItemToObject(object, name, cJSON_CreateString(s))
  cJSON_AddStringToObject(json, "email", "john.doe@example.com");

  // char * cJSON_PrintUnformatted (cJSON *item)
  char *json_str = cJSON_PrintUnformatted(json);

  //  char *strncpy(char dst[restrict .sz], const char *restrict src, size_t sz);
  strncpy(buffer, json_str, BUFFER_SIZE - 1);
  buffer[BUFFER_SIZE -1] = '\0';

  //  void(* cJSON_free)(void *ptr) = free [static]
  free(json_str);

  //  void cJSON_Delete (cJSON *c)
  cJSON_Delete(json);
}

void createMainPkt_B(char *buffer, int seq, time_t current_time, sensorData *sensor_data, int sensor_count)
{
  //  cJSON * cJSON_CreateObject ()
  cJSON *json = cJSON_CreateObject();

  char message[25];  // character message buffer size store the 25

  //  int sprintf(char *restrict str, const char *restrict format, ...);
  sprintf(message, "%ld000", current_time);

  //  cJSON_AddStringToObject(object, name, s) cJSON_AddItemToObject(object, name, cJSON_CreateString(s))
  cJSON_AddStringToObject(json, "timestamp", message);
  cJSON_AddStringToObject(json, "customerid", B_CUSTOMER_ID);
  cJSON_AddStringToObject(json, "tenantid", B_TENANT_ID);
  cJSON_AddStringToObject(json, "deviceid", B_DEVICE_ID);
  cJSON_AddStringToObject(json, "datatype", "metadata");
  cJSON_AddStringToObject(json, "datamode", "monitoring");

  //  cJSON * 	cJSON_CreateArray ()
  cJSON *data_array = cJSON_CreateArray();

  cJSON_AddItemToObject(json, "equipment", data_array);

  for (int i = 0; i < sensor_count; i++)
  {
    // this function call B_sensor
    B_sensor(data_array, i, &sensor_data[i], current_time, NULL);
  }

  /** timestamp for cJSON printing formating **/
  // timestamp create for cJSON_PrintPreallocated this function 
  cJSON_PrintPreallocated(json, buffer, BUFFER_SIZE, 0);

  //  void cJSON_Delete (cJSON *c)
  cJSON_Delete(json);
}
