#ifndef _DATA_H
#define _DATA_H

#include <stdint.h>
#include <time.h>

// typedef for struct name VavSensorData
typedef struct 
{
  // VAV Thermostat this all parameter names
  uint16_t device_id;	//  Reg 0
  uint16_t vav_status;	//  Reg 1	VAS 0 = OFF, 1 = ON 		//  Auto Manual  ---> vas this register for mqtt read / write
  uint16_t v2s_source;	//  Reg 2	V2S
  uint16_t mode;	//  Reg 3 	MOD 0 = Auto, 1 = Manual	//  Auto Manual  ---> mod this register for mqtt read / write
  uint16_t cfm;		//  Reg 4	
  uint16_t ccf;		//  Reg 5
  uint16_t vnm;		//  Reg 6	Vnom 
  uint16_t vmn;		//  Reg 7	Vmin
  uint16_t vmx;		//  Reg 8	Vmax
  uint16_t dmx;		//  Reg 9	deltaP 
  uint16_t dmp;		//  Reg 10	Damper
  uint16_t set_temp;	//  Reg 11	st2  set temperature		//  Auto Manual  ---> st2 this register for mqtt read / write
  uint16_t amb_temp;	//  Reg 12	AMB ambient temperature 
  uint16_t pir_status;	//  Reg 13	PIR
  uint16_t amb_temp_offset;	//  Reg 14	OFS
  uint16_t pfc_control;		//  Reg 15	PFC
  uint16_t direction;         	//  Reg 16  	0 = Forward, 1 = Reverse    // added (buf[16])
  uint16_t set_damper_position; //  Reg 17    set damper		//  Manual	---> set damper this register for mqtt read / write
} VavSensorData;

//  print the register function
void print_register(int idx, uint16_t val);

//  create json format for data function 
void createVavJson(char *buf, const VavSensorData *sensor, int count, time_t timestamp);

#endif
