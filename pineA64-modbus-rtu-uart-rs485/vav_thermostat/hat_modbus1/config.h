#ifndef CONFIG_H
#define CONFIG_H

#define DEVICE		"/dev/ttyS2"
#define BAUD_RATE 	19200
#define PARITY		'N'
#define DATA_BITS 	8
#define STOP_BITS	1
#define SLAVE_ID	1
#define NUM_REGS	18

#define DE_GPIO_CHIP 	"/dev/gpiochip0"
#define DE_GPIO_LINE	79	// GPIO79 = PC15 (RS485 DE/RE pin)

#endif // CONFIG_H
