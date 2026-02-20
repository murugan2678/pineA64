#include "boodskap.h"  // boodskap.h include function 
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// this for equipment id create 
uint16_t sen_vav_id[] = {458};	//  ZBEID0458
static char message[300];    // create for char message in mqtt char size 300 buf size

/*void B_sensor(cJSON *root_array, uint8_t sensor_idx, const VavSensorData *vav, time_t timestamp, const char *id_str)
  {
  if (sensor_idx != 0) {
  return;
  }

  cJSON *json = cJSON_CreateObject();

// Equipment ID (string)
snprintf(message, sizeof(message), "ZBEID%04d", sen_vav_id[sensor_idx]);
cJSON_AddStringToObject(json, "id", message);

// Integer / boolean-like fields as numbers
cJSON_AddNumberToObject(json, "VAS", vav->vav_status);      // usually 0/1
cJSON_AddNumberToObject(json, "V2S", vav->v2s_source);
cJSON_AddNumberToObject(json, "MOD", vav->mode);
cJSON_AddNumberToObject(json, "CFM", vav->cfm);
cJSON_AddNumberToObject(json, "CCF", vav->ccf);
cJSON_AddNumberToObject(json, "PIR", vav->pir_status);
cJSON_AddNumberToObject(json, "PFC", vav->pfc_control);
cJSON_AddNumberToObject(json, "dir", vav->direction);

// Floating point fields as numbers
cJSON_AddNumberToObject(json, "VNM", vav->vnm / 1.0);         // 550.0
cJSON_AddNumberToObject(json, "VMN", vav->vmn / 1.0);         // 200.0
cJSON_AddNumberToObject(json, "VMX", vav->vmx / 1.0);         // 300.0
cJSON_AddNumberToObject(json, "DMX", vav->dmx / 100.0);       // 300.00
cJSON_AddNumberToObject(json, "DMP", vav->dmp / 1.0);
cJSON_AddNumberToObject(json, "sT2", vav->set_temp / 100.0);  // 25.30
cJSON_AddNumberToObject(json, "AMB", vav->amb_temp / 100.0);  // e.g. 614.40 → probably wrong scaling? check if should be /10 or something
cJSON_AddNumberToObject(json, "OFS", vav->amb_temp_offset / 100.0);
cJSON_AddNumberToObject(json, "sps", vav->set_damper_position / 1.0);  // 99.0

cJSON_AddItemToArray(root_array, json);
} */


//  this function for main. json data print and array 
void B_sensor(cJSON *root_array, uint8_t sensor_idx, const VavSensorData *vav, time_t timestamp, const char *id_str)
{
  (void)timestamp;
  (void)id_str;

  if ( sensor_idx != 0)
  {
    return;
  }

  cJSON *json = cJSON_CreateObject();


  //  Equipment ID
  //  int snprintf(char *str, size_t size, const char *format, ...);
  snprintf(message, sizeof(message), "ZBEID%04d", sen_vav_id[sensor_idx]);
  cJSON_AddStringToObject(json, "id", message);

  //  All values as strings. vav thermostat mnemonics each register seprate mnemonics there that mnemonics give
  snprintf(message, sizeof(message), "%u", vav->vav_status);
  cJSON_AddStringToObject(json, "vas", message);			//  this for vav theremostat device mnemonics =======> "vas" 

  snprintf(message, sizeof(message), "%u", vav->v2s_source);
  cJSON_AddStringToObject(json, "v2s", message);

  snprintf(message, sizeof(message), "%u", vav->mode);
  cJSON_AddStringToObject(json, "mod", message);

  snprintf(message, sizeof(message), "%u", vav->cfm);
  cJSON_AddStringToObject(json, "cfm", message);

  snprintf(message, sizeof(message), "%u", vav->ccf);
  cJSON_AddStringToObject(json, "ccf", message);

  snprintf(message, sizeof(message), "%.1f", (double)vav->vnm / 1.0);
  cJSON_AddStringToObject(json, "vnm", message);

  snprintf(message, sizeof(message), "%.1f", (double)vav->vmn / 1.0);
  cJSON_AddStringToObject(json, "vmn", message);

  snprintf(message, sizeof(message), "%.1f", (double)vav->vmx / 1.0);
  cJSON_AddStringToObject(json, "vmx", message);

  snprintf(message, sizeof(message), "%.2f", (double)vav->dmx / 100.0);
  cJSON_AddStringToObject(json, "dmx", message);

  snprintf(message, sizeof(message), "%.1f", (double)vav->dmp / 1.0);
  cJSON_AddStringToObject(json, "dmp", message);

  snprintf(message, sizeof(message), "%.2f", (double)vav->set_temp / 100.0);
  cJSON_AddStringToObject(json, "st2", message);

  snprintf(message, sizeof(message), "%.2f", (double)vav->amb_temp / 100.0);
  cJSON_AddStringToObject(json, "amb", message);

  snprintf(message, sizeof(message), "%u", vav->pir_status);
  cJSON_AddStringToObject(json, "pir", message);

  snprintf(message, sizeof(message), "%.2f", (double)vav->amb_temp_offset / 100.0);
  cJSON_AddStringToObject(json, "ofs", message);

  snprintf(message, sizeof(message), "%u", vav->pfc_control);
  cJSON_AddStringToObject(json, "pfc", message);

  //snprintf(message, sizeof(message), "%u", vav->direction);
  //cJSON_AddStringToObject(json, "dir", message);

  snprintf(message, sizeof(message), "%.1f", (double)vav->set_damper_position / 1.0);
  cJSON_AddStringToObject(json, "sps", message);

  cJSON_AddItemToArray(root_array, json); // this create for one array json formate. inside all mnemonics parameters and registors comming
} 
