#include <unistd.h>
#include <modbus/modbus.h>
#include <gpiod.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "gpio_control.h"
#include "config.h"

struct gpiod_chip *chip = NULL;
struct gpiod_line *de_line = NULL;

void init_gpio()
{
  // struct gpiod_chip *gpiod_chip_open(const char *path);
  chip = gpiod_chip_open(DE_GPIO_CHIP);
  if(!chip)
  {
    perror("gpiod chip not open");
    exit(EXIT_FAILURE);
  }

  // struct gpiod_line *gpiod_chip_get_line(struct gpiod_chip *chip, unsigned int offset)
  de_line = gpiod_chip_get_line(chip, DE_GPIO_LINE);
  if(!de_line)
  {
    perror("gpiod chip get line not come");
    exit(EXIT_FAILURE);
  }

  //  int gpiod_line_request_output(struct gpiod_line *line, const char *consumer, int default_val)
  int ret = gpiod_line_request_output(de_line, "modbus_de", 0);
  if (ret < 0)
  {
    perror("gpio line request output not come");
    exit(EXIT_FAILURE);
  }
}

void rts_control(modbus_t *modbusRtu, int on)
{
  (void)modbusRtu;
  printf("RTS %s\n", on ? "HIGH (TX)" : "LOW (RX)");

  // int gpiod_line_set_value(struct gpiod_line *line, int value);
  gpiod_line_set_value(de_line, on);
  // usleep(100);
  usleep(on ? 1000 : 500);   // longer delay after going HIGH
}
