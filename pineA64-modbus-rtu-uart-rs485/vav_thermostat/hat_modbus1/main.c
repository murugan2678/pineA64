// write a program for UART2 communicate modbus RS485 serial mode. pineA64 board connect to the modbus RS485 GPIO pin communicate

#include <unistd.h>
#include <modbus/modbus.h>
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include "gpio_control.h"
#include "config.h"


int main()
{
  modbus_t *modbusRtu;
  uint16_t buf[18];

  int slave, modbusCon, modbusRead;

  init_gpio();

  // modbus_t *modbus_new_rtu(const char *device, int baud, char parity, int data_bit, int stop_bit);
  modbusRtu = modbus_new_rtu(DEVICE, BAUD_RATE, PARITY, DATA_BITS, STOP_BITS);
  if (modbusRtu == NULL)
  {
    perror("Unable to create modbus");
    exit(EXIT_FAILURE);
  }

  // int modbus_set_slave(modbus_t *ctx, int slave);
  slave = modbus_set_slave(modbusRtu, SLAVE_ID);
  if (slave == -1)
  {
    perror("Slave id not set");
    exit(EXIT_FAILURE);
  }

  // int modbus_set_debug(modbus_t *ctx, int flag);
  modbus_set_debug(modbusRtu, TRUE);

  // int modbus_set_response_timeout(modbus_t *ctx, uint32_t to_sec, uint32_t     t    o_usec);
  modbus_set_response_timeout(modbusRtu, 3, 0);

  // int modbus_rtu_set_rts(modbus_t *ctx, int mode)
  modbus_rtu_set_rts(modbusRtu, MODBUS_RTU_RTS_UP);

  // int modbus_rtu_set_custom_rts(modbus_t *ctx, void (set_rts) (modbus_t ctx,     int on))
  modbus_rtu_set_custom_rts(modbusRtu, rts_control);

  // int modbus_connect(modbus_t *ctx);
  modbusCon = modbus_connect(modbusRtu);
  if (modbusCon == -1)
  {
    perror("modbus not connect");
    exit(EXIT_FAILURE);
  }
  
  while(1)
  {
    printf("\n\n******* VAV Thermostate Datas *******\n\n");

    // int modbus_read_input_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest);
    modbusRead = modbus_read_registers(modbusRtu, 0, NUM_REGS, buf);

    if (modbusRead == -1)
    {
      perror("modbus not read registers");
      exit(EXIT_FAILURE);
    }
    else
    {
      // modbus data print
      for (int i = 0; i < modbusRead; i++)
      {
	vavThermostat(i, buf);
      }
      sleep(5);
    }
    printf("\n\n");
  }
  // void modbus_close(modbus_t *ctx);
  modbus_close(modbusRtu); 

  // void modbus_free(modbus_t *ctx);
  modbus_free(modbusRtu);

  gpiod_line_release(de_line);
  gpiod_chip_close(chip);
}

int vavThermostat(int i, uint16_t buf[])
{
  switch(i)
  {
    case 0:
      printf("Reg[%d] = %d ---> Device Mode (Auto/Manual/Heat/cool)\n",i , buf[i]);
      break;

    case 1:
      printf("Reg[%d] = %d ---> Fan Mode/State\n", i, buf[i]);
      break;

    case 2:
      printf("Reg[%d] = %d ---> Setpoint Temperature\n", i, buf[i]); 
      break;

    case 3:
      printf("Reg[%d] = %d ---> Current mode/state\n", i, buf[i]);
      break;

    case 4:
      printf("Reg[%d] = %d ---> Occupancy\n", i, buf[i]);
      break;

    case 5:
      printf("Reg[%d] = %d ---> Alarm/Error flags\n", i, buf[i]);
      break;

    case 6:
      printf("Reg[%d] = %1.f \u00B0C ---> Room Temperature\n", i, buf[i] / 10.0);
      break;

    case 7:
      printf("Reg[%d] = %1.f \u00B0C ---> Cool Setpoint\n", i, buf[i] / 10.0);
      break;

    case 8:
      printf("Reg[%d] = %1.f \u00B0C ---> Heat Setpoint\n", i, buf[i] / 10.0);
      break;

    case 9:
      printf("Reg[%d] = %1.f \u00B0C ---> Min damper position\n", i, buf[i] / 10.0);
      break;

    case 10:
      printf("Reg[%d] = %1.f \u00B0C ---> Max damper position\n", i, buf[i] / 10.0);
      break;

    case 11:
      printf("Reg[%d] = %d ---> CO2 ppm\n", i, buf[i]);
      break;

    case 12:
      printf("Reg[%d] = %1.f ---> Possibly total runtime\n", i, buf[i] / 10.0);
      break;

    case 13:
      printf("Reg[%d] = %d ---> Output active flag/stage\n", i, buf[i]);
      break;

    case 14:
      printf("Reg[%d] = %d ---> Airflow setpoint/measured(CFM)\n", i, buf[i]);
      break;

    case 15:
      printf("Reg[%d] = %d ---> Unused or status\n", i, buf[i]);
      break;

    case 16:
      printf("Reg[%d] = %d ---> Unused or status\n", i, buf[i]);
      break;

    case 17:
      printf("Reg[%d] = %.1f%% ---> Damper position\n", i, buf[i] / 10.0);
      break;

    default:
      break;
  }
  return 0;
}

/* output 

zedbee@pine64:~/vav_thermostat/hat_modbus_pineboard/hat_modbus1$ vi Makefile

zedbee@pine64:~/vav_thermostat/hat_modbus_pineboard/hat_modbus1$ make
gcc -Wall -Wextra -c main.c -o main.o
gcc -Wall -Wextra -c gpio_control.c -o gpio_control.o
gcc -o hat_modbus main.o gpio_control.o -lmodbus -lgpiod

zedbee@pine64:~/vav_thermostat/hat_modbus_pineboard/hat_modbus1$ make clean
rm -rf main.o gpio_control.o # hat_modbus

zedbee@pine64:~/vav_thermostat/hat_modbus_pineboard/hat_modbus1$ ls
config.h  gpio_control.c  gpio_control.h  hat_modbus  main.c  Makefile

zedbee@pine64:~/vav_thermostat/hat_modbus_pineboard/hat_modbus1$ sudo ./hat_modbus
i[sudo] password for zedbee:
Opening /dev/ttyS2 at 19200 bauds (N, 8, 1)


******* VAV Thermostate Datas *******

[01][03][00][00][00][12][C5][C7]
Sending request using RTS signal
RTS HIGH (TX)
RTS LOW (RX)
Waiting for a confirmation...
<01><03><24><00><01><00><01><00><00><00><00><00><00><00><00><02><26><00><64><01><2C><00><A0><00><C8><0A><82><0B><2E><00><01><03><E8><00><00><00><00><00><63><66><91>
Reg[0] = 1 ---> Device Mode (Auto/Manual/Heat/cool)
Reg[1] = 1 ---> Fan Mode/State
Reg[2] = 0 ---> Setpoint Temperature
Reg[3] = 0 ---> Current mode/state
Reg[4] = 0 ---> Occupancy
Reg[5] = 0 ---> Alarm/Error flags
Reg[6] = 55 °C ---> Room Temperature
Reg[7] = 10 °C ---> Cool Setpoint
Reg[8] = 30 °C ---> Heat Setpoint
Reg[9] = 16 °C ---> Min damper position
Reg[10] = 20 °C ---> Max damper position
Reg[11] = 2690 ---> CO2 ppm
Reg[12] = 286 ---> Possibly total runtime
Reg[13] = 1 ---> Output active flag/stage
Reg[14] = 1000 ---> Airflow setpoint/measured(CFM)
Reg[15] = 0 ---> Unused or status
Reg[16] = 0 ---> Unused or status
Reg[17] = 9.9% ---> Damper position                                  */
