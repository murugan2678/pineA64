#ifndef _DATA_H
#define _DATA_H

#include <stdint.h>

typedef struct
{
  uint16_t carbonDioxide; // carbon dioxide ---> cotwo
  //uint16_t temperature; // temperature ---> temp
  float temperature; // temperature ---> temp
  uint16_t humidity; // humidity ---> hum
  uint16_t pressureTempFive; // pressure temperature five ---> pmtwofive
  uint16_t pressureTemp; // pressure temperature ---> pmten
}sensorData;

#endif
