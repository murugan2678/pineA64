#include "boodskap.h"

// Define arrays, Equipment ID ---> ZBEID0230, Equipment Name ---> Hw Desk. Equipment ID ---> ZBEID0335, Equipment Name ---> Iaq9Testing
uint16_t sen_co2_id[] = {230, 335};

// Define message buffer used in B_sensor
static char message[300];

void B_sensor(cJSON *root, uint8_t id, sensorData *co2_sen, time_t timestamp, char *id_str)
{

  /* printf("\nboodskap.c sen_co2_id[0] :%d\nsen_co2_id[1] :%d\nsen_co2_id[id] [%d]\n", sen_co2_id[0], sen_co2_id[id]);
  printf("-->id :%d\n", id);  */

  if (id < sizeof(sen_co2_id) / sizeof (sen_co2_id[0]) && sen_co2_id[id] != 0)
  {
    //  cJSON * cJSON_CreateObject ()
    cJSON *json = cJSON_CreateObject();

    /*  snprintf ---> formatted output conversion */
    //  int snprintf(char str[restrict .size], size_t size, const char *restrict format, ...);
    snprintf(message, sizeof(message), "ZBEID%04d", sen_co2_id[id]);

    //  #define cJSON_AddStringToObject(object, name, s)		   
    cJSON_AddStringToObject(json, "id", message);

    sprintf(message, "%d", co2_sen->carbonDioxide);
    cJSON_AddStringToObject(json, "cotwo", message);

    sprintf(message, "%.2f", co2_sen->temperature);
    cJSON_AddStringToObject(json, "temp", message);

    sprintf(message, "%d", co2_sen->humidity);
    cJSON_AddStringToObject(json, "hum", message);

    sprintf(message, "%d", co2_sen->pressureTempFive);
    cJSON_AddStringToObject(json, "pmtwofive", message);

    sprintf(message, "%d", co2_sen->pressureTemp);
    cJSON_AddStringToObject(json, "pmten", message);
    printf("\n\n");

    //  void cJSON_AddItemToArray (cJSON *array, cJSON *item)
    cJSON_AddItemToArray(root, json);
  }
}
