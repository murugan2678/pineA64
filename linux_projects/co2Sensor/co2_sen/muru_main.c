#include "config.h"
#include "co2_client.h"
#include "server.h"
#include "data.h"
#include "jsonify.h"
#include "mqtt.h"
#include "boodskap.h"

/*#include <pthread.h>
#include <modbus/modbus.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> */

#define MQTT_TOPIC "/BZHEZISEWY/device/ZBDID02BA2CA259E4/msgs/Gateway/1/101"  // mqtt topic 

#define MQTT_USERNAME "DEV_BZHEZISEWY"	// zedbee mqtt user name  ---> DEV_BZHEZISEWY
#define MQTT_PASSWORD "UUOuKQvTSUtv"	// zedbee.io mqtt password  ---> UUOuKQvTSUtv
#define BUFFER_SIZE 2048	 // buffer size store

#ifndef PUBLISH_INTERVAL
#define PUBLISH_INTERVAL 300   // mqtt data publish the 5 minutes
#endif

volatile sig_atomic_t server_started = 0;

// mqtt thread function this for server data read and data send Mqtt publish in zedbee cloud
void *mqtt_thread(void *arg) 
{
    printf("MQTT thread started\n");
    int seq = 0;
    MQTTClient client;
    modbus_t *local_ctx = NULL;
    int connected = 0;
    time_t last_reconnect_attempt = 0;
    time_t last_forward_attempt = 0;
    const int RECONNECT_INTERVAL = 30;	// wait 30 seconds
    const int FORWARD_INTERVAL = 60;	// store message ever 60 seconds

    /* mqtt init */
    if (MQTT_Init(&client) != 0) 
    {
        fprintf(stderr, "Failed to initialize MQTT client\n");
        return NULL;
    }

    // Print current working directory for debugging
   /*  char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) 
    {
        printf("Current working directory: %s\n", cwd);
    }
    else 
    {
        fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
    } */


    /* mqtt connect */

    // int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions * options)
    if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) == 0) 
    {
        connected = 1;
	printf("MQTT connection successful\n");
    }
    else
    {
        fprintf(stderr, "MQTT connection failed\n");
    }

    /* modbus new tcp server address IP 192.168.0.136, server port 5503 */
    //  modbus_t *modbus_new_tcp(const char *ip, int port);
    local_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
    if (local_ctx == NULL) 
    {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        if (connected) 
	{
            MQTT_Disconnect(&client);
        }
        return NULL;
    }

    /* modbus set slave id for local ctx server */
    //  int modbus_set_slave(modbus_t *ctx, int slave);
    if (modbus_set_slave(local_ctx, SERVER_SLAVE_ID) == -1) 
    {
        fprintf(stderr, "Invalid slave ID: %s\n", modbus_strerror(errno));
        modbus_free(local_ctx);
        if (connected) 
	{
            MQTT_Disconnect(&client);
        }
        return NULL;
    }

    /* modbus set response tiemout. for local_ctx server, i set 1 seconds */
    //  int modbus_set_response_timeout(modbus_t *ctx, uint32_t to_sec, uint32_t to_usec);
    modbus_set_response_timeout(local_ctx, 1, 0);
    if (modbus_connect(local_ctx) == -1) 
    {
        fprintf(stderr, "Initial Modbus connection failed: %s\n", modbus_strerror(errno));
    }

    time_t last_log_time = time(NULL);
    while (1) 
    {
        printf("MQTT thread loop iteration\n");

	/* current time taken */
        time_t current_time = time(NULL);

        if (current_time - last_log_time >= 30) 
	{
            printf("MQTT thread is active. Current time: %s", ctime(&current_time));
            last_log_time = current_time;
        }

        // Attempt reconnection if not connected
        if (!connected && (current_time - last_reconnect_attempt)) /************/
	{
	  printf("MQTT reconnection...\n");

	  if (MQTT_Init(&client) != 0)
	  {
	    fprintf(stderr, "Failed to reinitialize MQTT client\n");
	    last_reconnect_attempt = current_time;
	    sleep(5);
	    continue;
	  }
          //  int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions * options)	  
	  if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) == 0) 
	  {
    	    connected = 1;
	    // retry_count = 0;   // reset on success
	    printf("MQTT reconnected successfully\n");
	  } 
	  else 
	  {
    	    fprintf(stderr, "MQTT reconnection failed\n");
	    last_reconnect_attempt = current_time; /************/
	    // Continue to try publishing to trigger store_message
	  }
        }

	/* forward stored message if connected */
	if (connected && (current_time - last_forward_attempt))
	{
	  forward_stored_message(&client);
	  last_forward_attempt = current_time;
	}

        uint16_t reg[20];   //  reg[20] this for register buffer

	/* modbus read register for local_ctx server data. 0 to 20 register read a data */
	//  int modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest);
        int num_read = modbus_read_registers(local_ctx, 0, 20, reg);
        if (num_read == -1) 
	{
            fprintf(stderr, "Failed to read Modbus registers: %s\n", modbus_strerror(errno));

	    //  modbus close the local_ctx server
            modbus_close(local_ctx);
            if (modbus_connect(local_ctx) == -1) 
	    {
                fprintf(stderr, "Modbus reconnection failed: %s\n", modbus_strerror(errno));
            }
	   // last_publish_time = current_time;    /***/
            sleep(5);
            continue;
        }

	// server register data printing
        for (int i = 0; i < num_read; i++) 
	{
            printf("main.c mqtt send data buffer reg[%d] = %d\n", i, reg[i]);
        }

	// access the particular register data and send mqtt publish the data
        sensorData sensors[2];

	/* this for device one register access particluar register data send mqtt */
        sensors[0].carbonDioxide = reg[0];
        sensors[0].temperature = reg[1] / 100.0;
        sensors[0].humidity = reg[2];
        sensors[0].pressureTempFive = reg[3];
        sensors[0].pressureTemp = reg[4];

	// printf("==== sensor-temp reg[1] : %f\n", sensors->temperature = reg[1] / 100.0);

	/* this for device two register access particluar register data send mqtt */
        sensors[1].carbonDioxide = reg[10];
        sensors[1].temperature = reg[11] / 100.0;
        sensors[1].humidity = reg[12];
        sensors[1].pressureTempFive = reg[13];
        sensors[1].pressureTemp = reg[14];

	// Publish data sensor
        char json_buffer[BUFFER_SIZE];  // json_buffer size give 2048

	// data send mqtt, createMainPkt_B this function for jsonify.c this files inside there. this data send to createMainPkt_B in jsonify.c
        createMainPkt_B(json_buffer, seq++, current_time, sensors, 2);

        // Attempt publish regardless of connection status
	// int MQTTClient_publishMessage (MQTTClient handle, const char * topicName, MQTTClient_message * msg, MQTTClient_deliveryToken * dt)
        if (MQTT_Publish(&client, json_buffer, MQTT_TOPIC) != 0) 
	{
            fprintf(stderr, "Failed to publish sensor data\n");
	    if (connected)
	    {
	      connected = 0; // Mark as disconnected
              // modbus_close(local_ctx);
              MQTT_Disconnect(&client);
	    }
        }
       /*	else
       	{
            printf("Published to topic '%s': %s\n", MQTT_TOPIC, json_buffer);
        } */ 
	//last_publish_time = current_time;   /**************/
	sleep(PUBLISH_INTERVAL);  // 5 minutes once send data in mqtt publish data
    }

    /* free to modbus local ctx */

    //  void modbus_close(modbus_t *ctx);	
    modbus_close(local_ctx);

    //  void modbus_free(modbus_t *ctx);
    modbus_free(local_ctx);
    if (connected) 
    {
       /* mqtt disconnect clien */
       MQTT_Disconnect(&client);
    }
    return NULL;
}

/* main function */
int main()
{
#ifdef _WIN32

  WSADATA wsaData;

  /* this for windows server */
  if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR)
  {
    log_error("WSAStartup failed with error %d", WSAGetLastError());
    perror("WSAStartup failed with error %d", WSAGetLastError());
    WSACleanup();
    return 1;
  }

#endif
    /* char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) 
    {
        printf("Main: Current working directory: %s\n", cwd);
    }
    else
    {
        fprintf(stderr, "Main: getcwd failed: %s\n", strerror(errno));
    }  */

    /* read config funciton failed means return 1 */
    if (read_config_file() != 0) 
    {
        fprintf(stderr, "Failed to read configuration file\n");
        return 1;
    }

    // pthread declare the variables
    pthread_t co2_tid, server_tid, mqtt_tid;
     
    /*  pthread create for cilent */
    //  int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg);
    if (pthread_create(&co2_tid, NULL, co2_client_thread, NULL) != 0) 
    {
        fprintf(stderr, "Failed to create CO2 client thread\n");
        return 1;
    }

    /*  pthread create for server  */
    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) 
    {
        fprintf(stderr, "Failed to create server thread\n");
        return 1;
    }

    /*  pthread create for mqtt */
    if (pthread_create(&mqtt_tid, NULL, mqtt_thread, NULL) != 0) 
    {
        fprintf(stderr, "Failed to create MQTT thread\n");
        return 1;
    }

    //  int pthread_join(pthread_t thread, void **retval);
    pthread_join(co2_tid, NULL);	//  pthread join for client
    pthread_join(server_tid, NULL);	//  pthread join for server
    pthread_join(mqtt_tid, NULL);	//  pthread join for mqtt
    return 0;

#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}
