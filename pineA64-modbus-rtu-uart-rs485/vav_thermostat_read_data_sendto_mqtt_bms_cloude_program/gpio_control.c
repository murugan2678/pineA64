#include <unistd.h>
#include <modbus/modbus.h>
#include <gpiod.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "gpio_control.h"  // gipo files header config
#include "config.h"	// all header configuration files

struct gpiod_chip *chip = NULL; 	//  declare for the gpiod chip with pionter variable. pineA64 pins enable method this wave only
struct gpiod_line *de_line = NULL;      //  declare for the gpiod line with pionter variable 


/* The gpio initailized function */
void init_gpio()
{

  /*  The gpiochip0 in DE pins enable select this DE_GPIO_CHIP ---> "/dev/gpiochip0"  */
  // struct gpiod_chip *gpiod_chip_open(const char *path);
  chip = gpiod_chip_open(DE_GPIO_CHIP);
  if(!chip)
  {
    perror("gpiod chip not open");
    exit(EXIT_FAILURE);
  }

  /*  chip fd pass to chip get line. uart 3 pin DE2 pin for = PD6 // one calculation there value 102 ---> GPIO102 = PD6 (RS485 DE/RE pin), ---> Euler "e" Connector  */
								  // struct gpiod_line *gpiod_chip_get_line(struct gpiod_chip *chip, unsigned int offset)
  de_line = gpiod_chip_get_line(chip, DE_GPIO_LINE);
  if(!de_line)
  {
    perror("gpiod chip get line not come");
    exit(EXIT_FAILURE);
  }

  /* de_line fd pass to line request */
  //  int gpiod_line_request_output(struct gpiod_line *line, const char *consumer, int default_val)
  int ret = gpiod_line_request_output(de_line, "modbus_de", 0);
  if (ret < 0)
  {
    perror("gpio line request output not come");
    exit(EXIT_FAILURE);
  }
}


//  rts control for rts uart tx and rx
void rts_control(modbus_t *modbusRtu, int on)
{

  (void)modbusRtu;
  if (de_line == NULL)
  {
    fprintf(stderr, "ERROR: de_line is NULL in rts_control! GPIO not initialized?\n");
    return;  // or exit(1) if you prefer hard fail
  }

  (void)modbusRtu;
  printf("RTS %s\n", on ? "HIGH (TX)" : "LOW (RX)");

  /* this function for on ---> TX means High means 1 and RX Low means 1 */
  // int gpiod_line_set_value(struct gpiod_line *line, int value);
  int ret = gpiod_line_set_value(de_line, on);

  if (ret < 0)
  {
    perror("gpiod_line_set_value failed");
  }

  /* wait to communicate next */
  // usleep(100);
  usleep(on ? 1000 : 500);   // longer delay after going HIGH
}
