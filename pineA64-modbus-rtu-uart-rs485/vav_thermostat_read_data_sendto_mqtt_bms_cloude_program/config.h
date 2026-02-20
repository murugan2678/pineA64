#ifndef CONFIG_H
#define CONFIG_H


#include <modbus/modbus.h>
#include <unistd.h>

#define DEVICE		"/dev/ttyS3" 	//  this for serial modbus rtu usb connect "/dev/ttyUSB0",
					//  uart 2 this for serial modbus rtu with pine hat board // "/dev/ttyS2"  // 8 --> UART2_TXD GPIO 14 PB0, 10 --> UART_RXD GPIO 15 PB1 ---> Pi-2 Connector
					//  uart 3 this for serial modbus rtu with pine hat board // "/dev/ttyS3"  // 24 --> UART3_TXD PD0, 23 --> UART3_RXD PD1  ---> Euler "e" Connector
					//  uart 4 this for serial modbus rtu with pine hat board // "/dev/ttyS4"  // 19 --> UART4_TXD PD2, 21 --> UART4_RXD PD3  ---> Euler "e" Connector

#define BAUD_RATE 	19200		//  Baud Rate ---> 19200
#define PARITY		'N'		//  Parity for N ---> None
#define DATA_BITS 	8		//  Data Bits ---> 8
#define STOP_BITS	1		//  Stop Bits ---> 1
#define SLAVE_ID	2  // 12	//  Slave ID ---> 6
#define NUM_REGS	18		//  Number of Register ---> 18

#define DE_GPIO_CHIP 	"/dev/gpiochip0"  // this for gpiochip0 in DE pins enable select this 
#define DE_GPIO_LINE	102 	// this for uart 3 DE2 pin for = PD6  // 102 ---> // GPIO102 = PD6 (RS485 DE/RE pin), ---> Euler "e" Connector  
				// this for uart 4 DE3 pin for = PD4  // 100 ---> // GPIO100 = PD4 (RS485 DE/RE pin), ---> Euler "e" Connector
				// this for uart 2 DE1 pin for = PC15 //  79 ---> // GPIO79 = PC15 (RS485 DE/RE pin)  ---> Pi-2 Connector

#define BUFFER_SIZE1 30000	// Buffer size 30000


// Boodskap / MQTT
#define BROKER_DOMAIN_KEY   "BZHEZISEWY"
#define BROKER_API_KEY      "Yrzp62Nu20H0" // "Yrzp62Nu20H0" ---> this for v4dev in iitmrp    // "UUOuKQvTSUtv" this for zedbee.io in production main
#define BOODSKAP_BROKER     "v4dev.zedbee.in" // tcp://v4dev.zedbee.in:1883 ---> this for v4dev in iitmrp  // "zedbee.io" ---> this for BMS cloude main
#define BOODSKAP_USERNAME   "DEV_" BROKER_DOMAIN_KEY  	//  boodskap username ---> "DEV_BZHEZISEWY"
#define BOODSKAP_PASSWORD   BROKER_API_KEY	 	//  boodskap password ---> "Yrzp62Nu20H0"


#define B_CUSTOMER_ID       "ZBCID00012"  // "ZBCID00030". customer id will be changed
#define B_TENANT_ID         "ZBTID0001"	  // "ZBTID0001". Tenant id not changed
#define B_DEVICE_ID         "ZBDID02BA86068689"	 // ZBDID02BA86068689". device id will be changed

#define BOODSKAP_PUBLISH_TOPIC  "/BZHEZISEWY/device/ZBDID02BA86068689/msgs/Gateway/1/101"// "/" BROKER_DOMAIN_KEY "/device" B_DEVICE_ID "/msgs/gateway/1/101"
#define BOODSKAP_SUBSCRIBE_TOPIC "/BZHEZISEWY/device/ZBDID02BA86068689/cmds"  // "/" BROKER_DOMAIN_KEY "/device/" B_DEVICE_ID "/cmds"
#define BOODSKAP_ACK_TOPIC       "/BZHEZISEWY/device/ZBDID02BA86068689/msgs/Gateway/1/103" // "/" BROKER_DOMAIN_KEY "/device/" B_DEVICE_ID "/msgs/gateway/1/103"

#define CLIENTID            "DEV_" B_DEVICE_ID   // "DEV_ZBDID02BA86068689"  device id will be changed
#define PUBLISH_INTERVAL    180


#endif // CONFIG_H
