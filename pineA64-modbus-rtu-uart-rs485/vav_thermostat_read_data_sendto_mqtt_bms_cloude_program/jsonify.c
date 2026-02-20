#include "jsonify.h"	//  header file include for jsonify.h, json function 
#include "mqtt.h"	//  header file include for mqtt.h, mqtt funcion
#include "boodskap.h"   //  boodskap.h send data function
#include "data.h"	//  data.h vav register structure
#include "cjson/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>	// -lm mathametic inlcude use means this header use

#define B_CUSTOMER_ID "ZBCID00012" // "ZBCID00030" 	//  Customer id will be changed
#define B_TENANT_ID "ZBTID0001"		//  Tenant id	//  Tenant id not changed
#define B_DEVICE_ID "ZBDID02BA86068689" //  Device id	//  Device id will be changed

#define BUFFER_SIZE 2048		//  Buffer size 

extern time_t last_mqtt_sysnc;  // declare for the last mqtt
extern int current_publish_status;  //  declare for the current publish

/* This function for create buffer default setup for jsonify */
void creatTestPkt(char *buffer)
{
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "name", "John Doe");
  cJSON_AddNumberToObject(json, "age", 30);
  cJSON_AddStringToObject(json, "email", "john.doe@example.com");

  char *json_str = cJSON_PrintUnformatted(json);

  //  char *strncpy(char *dest, const char *src, size_t n);
  strncpy(buffer, json_str, BUFFER_SIZE - 1);

  buffer[BUFFER_SIZE -1] = '\0';
  free(json_str);
  cJSON_Delete(json);
}

//  void createVavJson(char *buffer, sensorVavData *vav_data, time_t current_time, int sensor_count)
void createVavJson(char *buffer, const VavSensorData *vav_data, int sensor_count, time_t current_time)
{
  //  create json for this
  cJSON *json = cJSON_CreateObject();
  char message[32];  // character message 

  //  int sprintf(char *str, const char *format, ...);  
  sprintf(message, "%ld000", current_time);

  //  This for timestamp and device releted name print for json formate
  cJSON_AddStringToObject(json, "timestamp", message);  
  cJSON_AddStringToObject(json, "customerid", B_CUSTOMER_ID);
  cJSON_AddStringToObject(json, "tenantid", B_TENANT_ID);
  cJSON_AddStringToObject(json, "deviceid", B_DEVICE_ID);
  cJSON_AddStringToObject(json, "datatype", "metadata"); // "vav_thermostat";
  cJSON_AddStringToObject(json, "datamode", "monitoring");

  // cjson create array. inside data
  cJSON *equipment_array = cJSON_CreateArray();

  // equipment id
  cJSON_AddItemToObject(json, "equipment", equipment_array);

  for (int i = 0; i < sensor_count; i++)
  {
    //  this function send boodskap.c. registers and timestamp
    B_sensor(equipment_array, i, &vav_data[i], current_time, NULL);
  }

  /* char *json_str = cJSON_PrintUnformatted(json);
     strncpy(buffer, json_str, BUFFER_SIZE - 1);
     buffer[BUFFER_SIZE - 1] = '\0';
     free(json_str);
     cJSON_Delete(json); */

  // cJSON_AddItemToObject(json, buffer, BUFFER_SIZE, 0);
  /* === FIXED: Convert to string and copy to buffer === */
  char *json_str = cJSON_PrintUnformatted(json);
  if (json_str)
  {
    strncpy(buffer, json_str, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    free(json_str);
  } 
  else 
  {
    buffer[0] = '\0';
  }

  cJSON_Delete(json); 
}



/*  This function create for character buffer and data buf for main */
/* void createVAVJson(char *buffer, uint16_t *buf)
   {
//  create for json object
cJSON *json = cJSON_CreateObject();

//  current time 
time_t now;

//  current time come for here
time(&now);

//  character ts buffer create for [25] buf size for 25
char ts[25];

//  int sprintf(char *str, const char *format, ...);
sprintf(ts, "%ld000", now);	//  this for timestamp for current time passed to ---> ts variable

//  This for timestamp and device releted name print for json formate
cJSON_AddStringToObject(json, "timestamp", ts);
cJSON_AddStringToObject(json, "customerid", B_CUSTOMER_ID);
cJSON_AddStringToObject(json, "tenantid", B_TENANT_ID);
cJSON_AddStringToObject(json, "deviceid", B_DEVICE_ID);
cJSON_AddStringToObject(json, "datatype", "vav_thermostat");
cJSON_AddStringToObject(json, "datamode", "monitoring");


//  This for VAV thermostat for all paramete mnemonics create the json formate in vav thermostat data 

//  cJSON_AddNumberToObject(json, "slave_id", buf[0]);

cJSON_AddNumberToObject(json, "VAS", buf[1]);
cJSON_AddNumberToObject(json, "V2S", buf[2]);
cJSON_AddNumberToObject(json, "MOD", buf[3]);
cJSON_AddNumberToObject(json, "CFM", buf[4]);
cJSON_AddNumberToObject(json, "CCF", buf[5]);
cJSON_AddNumberToObject(json, "VNM", (double)buf[6] / 1.0);
cJSON_AddNumberToObject(json, "VMN", (double)buf[7] / 1.0);
cJSON_AddNumberToObject(json, "VMX", (double)buf[8] / 1.0);
cJSON_AddNumberToObject(json, "DMX", (double)buf[9] / 100.0);
cJSON_AddNumberToObject(json, "DMP", (double)buf[10] / 1.0);
cJSON_AddNumberToObject(json, "sT2", (double)buf[11] / 100.0);
cJSON_AddNumberToObject(json, "AMB", (double)buf[12] / 100.0);
cJSON_AddNumberToObject(json, "PIR", buf[13]);
cJSON_AddNumberToObject(json, "OFS", (double)buf[14] / 100.0);
cJSON_AddNumberToObject(json, "PFC", buf[15]);

//  cJSON_AddNumberToObject(json, "direction", buf[16]);

cJSON_AddNumberToObject(json, "SPS", (double)buf[17] / 1.0);

//   This for create in allocate data buffer create to print json formate  
cJSON_PrintPreallocated(json, buffer, BUFFER_SIZE, 0);

// once create mqtt publish data for json formate. mqtt data publish after delete for json formate. next time create new json formate 
cJSON_Delete(json);
} */
