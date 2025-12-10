#ifndef CONFIG_H
#define CONFIG_H

#include <unistd.h> 	// POSIX system calls like ---> close, read, write.
#include <stdio.h> 	// Input/output functions like ---> printf, scanf, fopen
#include <stdlib.h> 	// memory allocation ---> malloc, free
#include <string.h> 	// string manipulation function like ---> strlen, strcpy, strcmp
#include <stdint.h> 	// integer types ---> int8_t uint32_t
#include <stdarg.h> 	// handling variable argument lists
#include <time.h> 	// manipulating date and time ---> time, ctime, clock
#include <pthread.h> 	// POSIX threads API for creating managing threads
#include <errno.h> 	// errno variable and error codes for systems call
#include <modbus/modbus.h> 	// Modbus protocol library, communication with industriak devices ---> PLCs, sensors
				// Modbus communication Ex : ---> reading/writing registers over TCP or RTU.
#ifdef _WIN32
#include <Winsock2.h> 	// Windows Sockets (Winsock) API for network programming, creating and managing sockets for
			// TCP / UDP communication
#include <WS2tcpip.h> 	// Winsock with additional TCP / Ip-sepcific functions. Ex ---> inet_pton, inet_ntop
#else
#include <sys/socket.h> 	// socket API for creating and managing network sockets on Unix-like systems
				// network communication. Ex ---> TCP / UDP sockets
#include <arpa/inet.h> 		// manipulatiing IP Addresses. Ex inet_addr, inet_ntoa
#include <netinet/in.h> 	// sturctures like sockaddr_in for IPv4 addressing
#endif

// File paths
#define DEV_FILE_PATH "/home/zedbee/co2_sen/configurations/device.csv" 	// this path for device.csv files waveshare ip set inside
#define ERROR_LOG_FILE_PATH "/home/zedbee/co2_sen/error.bin" 	// this path for error.bin inside error log print
#define LOG_FILE_PATH "/home/zedbee/co2_sen/log/" 	// this path for log directory create daily log files

#define MAX_DEVICES 2 		// Maximum devices
#define MAX_COLS 6 		// Maximum columns
#define MAX_LINE_SIZE 256 	// Maximum line size
#define MAX_INFO_MAX_COL 6 	// Maximum info maximum columns
#define MAX_REGISTERS 20 	// Maximum registers

// Device Structure
typedef struct		// This for typedef structure, typedef is existing data types
{
  uint16_t id; 	// Device ID
  char *device_IP; 	// Device IP Address. Ex ---> 192.168.0.200
  uint16_t device_port_number; 	// Device Port. Ex ---> 502
  uint8_t device_slave_ID; 		// Device Slave ID. Ex ---> 1
  uint8_t device_function_code; 	// Device Function Code. Ex ---> 0x03 reading holding registers
  uint16_t device_quantiy_to_read; 	// Device quantity. Ex ---> 8
} DeviceInfo; 		// typedef struct DeviceInfo

extern DeviceInfo devices[MAX_DEVICES]; 	// extern DeviceInfo add by device[MAX_DEVICES]
extern int total_devices;

extern modbus_mapping_t *mb_mapping; 		// modbus_mapping_t this for structure

// adding new
extern char *SERVER_ADDRESS;
extern int *SERVER_PORT;
extern int *SERVER_SLAVE_ID;

int read_config_file(void);


void log_error(const char *str, ...);
void log_data(const char *str, ...);

#endif /* CONFIG_H */
