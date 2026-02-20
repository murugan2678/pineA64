#ifndef CONFIG_H
#define CONFIG_H

#define DEVICE		"/dev/ttyS3" 	//  this for serial modbus rtu usb connect "/dev/ttyUSB0",
				 	//  uart 2 this for serial modbus rtu with pine hat board // "/dev/ttyS2"  // 8 --> UART2_TXD GPIO 14 PB0, 10 --> UART_RXD GPIO 15 PB1 ---> Pi-2 Connector
				 	//  uart 3 this for serial modbus rtu with pine hat board // "/dev/ttyS3"  // 24 --> UART3_TXD PD0, 23 --> UART3_RXD PD1  ---> Euler "e" Connector
				 	//  uart 4 this for serial modbus rtu with pine hat board // "/dev/ttyS4"  // 19 --> UART4_TXD PD2, 21 --> UART4_RXD PD3  ---> Euler "e" Connector
#define BAUD_RATE       19200
#define PARITY		'N'
#define DATA_BITS 	8
#define STOP_BITS	1
#define SLAVE_ID	2 // 6 // 1
#define NUM_REGS	18

#define DE_GPIO_CHIP 	"/dev/gpiochip0"  // this for gpiochip0 in DE pins enable select this 
#define DE_GPIO_LINE	102 	// this for uart 3 DE2 pin for = PD6  // 102 ---> // GPIO102 = PD6 (RS485 DE/RE pin), ---> Euler "e" Connector  
			    	// this for uart 4 DE3 pin for = PD4  // 100 ---> // GPIO100 = PD4 (RS485 DE/RE pin), ---> Euler "e" Connector
				// this for uart 2 DE1 pin for = PC15 //  79 ---> // GPIO79 = PC15 (RS485 DE/RE pin)  ---> Pi-2 Connector

#endif // CONFIG_H
