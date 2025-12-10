#include "boodskap.h"

// Define arrays, Equipment ID ---> ZBEID0230, Equipment Name ---> Hw Desk. Equipment ID ---> ZBEID0335, Equipment Name ---> Iaq9Testing

// this for iaq sensor outdoor.

uint16_t sen_co2_id[] = {481, 245, 475, 246};  // {245, 246}; // {230, 335};

// Define message buffer used in B_sensor
static char message[300];

void B_sensor(cJSON *root, uint8_t id, sensorData *co2_sen, time_t timestamp, char *id_str)
{

  /* printf("\nboodskap.c sen_co2_id[0] :%d\nsen_co2_id[1] :%d\nsen_co2_id[id] [%d]\n", sen_co2_id[0], sen_co2_id[id]);
  printf("-->id :%d\n", id);  */

  if (id < sizeof(sen_co2_id) / sizeof (sen_co2_id[0]) && sen_co2_id[id] != 0)
  {
    cJSON *json = cJSON_CreateObject();

    snprintf(message, sizeof(message), "ZBEID%04d", sen_co2_id[id]);
    cJSON_AddStringToObject(json, "id", message);

    printf("debug ---> B_sensor: id=%d (ZBEID%04d), carbonDioxide=%d, temperature=%.2f, humidity=%d, pressureTempFive=%d, pressureTemp=%d\n",
           id, sen_co2_id[id], co2_sen->carbonDioxide, co2_sen->temperature, co2_sen->humidity,
           co2_sen->pressureTempFive, co2_sen->pressureTemp);

    if (sen_co2_id[id] == 245 || sen_co2_id[id] == 246)
    {
      sprintf(message, "%d", co2_sen->carbonDioxide);
      cJSON_AddStringToObject(json, "cotwo", message);
      
      sprintf(message, "%d", co2_sen->pressureTempFive);
      cJSON_AddStringToObject(json, "pmtwofive", message);

      sprintf(message, "%d", co2_sen->pressureTemp);
      cJSON_AddStringToObject(json, "pmten", message); 
      //printf("\n\n");
    }

    else if (sen_co2_id[id] == 481 || sen_co2_id[id] == 475)
    {
      sprintf(message, "%.2f", co2_sen->temperature);
      cJSON_AddStringToObject(json, "temp", message);

      sprintf(message, "%d", co2_sen->humidity);
      cJSON_AddStringToObject(json, "hum", message);
      printf("\n\n");
    }

    cJSON_AddItemToArray(root, json);
  }
}
