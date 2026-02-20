#ifndef GPIOD_CONTROL_H
#define GPIOD_CONTROL_H

extern struct gpiod_chip *chip;
extern struct gpiod_line *de_line;

void init_gpio(void);
void rts_control(modbus_t *modbusRtu, int on);
int vavThermostat(int i, uint16_t buf[]);

#endif
