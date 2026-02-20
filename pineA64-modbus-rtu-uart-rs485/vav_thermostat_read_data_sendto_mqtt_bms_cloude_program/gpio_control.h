#ifndef GPIOD_CONTROL_H
#define GPIOD_CONTROL_H

extern struct gpiod_chip *chip;		//  gpiod chip structure
extern struct gpiod_line *de_line;	//  gpiod lline structure

void init_gpio(void);			//  intilaized gpio configuration
void rts_control(modbus_t *modbusRtu, int on);	//  modbus rtu control for PineA64 HatBoard Modbus Rtu 

// int vavThermostat(int i, uint16_t buf[]);	//  VAV thermostat function 

#endif
