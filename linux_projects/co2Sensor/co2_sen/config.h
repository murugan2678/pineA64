#ifndef CONFIG_H
#define CONFIG_H

#include <unistd.h> 	
#include <stdio.h> 	
#include <stdlib.h> 	
#include <string.h> 	
#include <stdint.h> 	
#include <stdarg.h> 	
#include <time.h>  
#include <pthread.h> 	
#include <errno.h> 	
#include <modbus/modbus.h> 	
				
#ifdef _WIN32
#include <Winsock2.h> 	
			
#include <WS2tcpip.h> 	
#else
#include <sys/socket.h> 	
			
#include <arpa/inet.h> 		
#include <netinet/in.h> 	
#endif

// File paths
#define DEV_FILE_PATH "/home/zedbee/co2_sen/configurations/device.csv" 	
#define ERROR_LOG_FILE_PATH "/home/zedbee/co2_sen/error.bin" 
#define LOG_FILE_PATH "/home/zedbee/log/" 	

#define MAX_DEVICES 2 		
#define MAX_COLS 6 		
#define MAX_LINE_SIZE 256 	
#define MAX_INFO_MAX_COL 6 	
#define MAX_REGISTERS 20 	

// Device Structure
typedef struct	
{
  uint16_t id; 	
  char *device_IP; 	
  uint16_t device_port_number; 	
  uint8_t device_slave_ID; 		
  uint8_t device_function_code; 	
  uint16_t device_quantiy_to_read; 	
} DeviceInfo; 		

extern DeviceInfo devices[MAX_DEVICES]; 	
extern int total_devices;
extern modbus_mapping_t *mb_mapping; 		
extern char *SERVER_ADDRESS;
extern int *SERVER_PORT;
extern int *SERVER_SLAVE_ID;

int read_config_file(void);
void log_error(const char *str, ...);
void log_data(const char *str, ...);

#endif /* CONFIG_H */
