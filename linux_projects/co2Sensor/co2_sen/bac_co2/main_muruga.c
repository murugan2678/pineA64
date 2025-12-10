#include "config.h"     // read_config_file
#include "co2_client.h" // co2_client_thread
#include "server.h"     // server_thread
#include "data.h"
#include "jsonify.h"
#include "mqtt.h"
#include "boodskap.h"
#include <pthread.h>
#include <modbus/modbus.h>

#define MQTT_TOPIC "/BZHEZISEWY/device/ZBDID02BA2CA259E4/msgs/Gateway/1/101"

#define MQTT_USERNAME "DEV_BZHEZISEWY"  // zedbee mqtt user name  ---> DEV_BZHEZISEWY
#define MQTT_PASSWORD "UUOuKQvTSUtv"    // zedbee.io mqtt password  ---> UUOuKQvTSUtv

#define BUFFER_SIZE 2048   // buffer size store

sensorData sensors[2] = {0};

// mqtt thread function this for server data read and data send Mqtt publish in zedbee cloud
void *mqtt_thread(void *arg)
{
  int seq = 0;
  MQTTClient client;

  /* mqtt init */
  if (MQTT_Init(&client) != 0) 
  {
    log_error("Failed to initialize MQTT client");
    return NULL;
  }

  /* mqtt connect */
  if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) != 0) 
  {
    log_error("Failed to connect to MQTT broker");
    MQTT_Disconnect(&client);
    return NULL;
  }

  /* modbus new tcp server address IP 192.168.0.136, server port 5503 */
  modbus_t *local_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);
  if (local_ctx == NULL) 
  {
    log_error("Unable to allocate libmodbus for local");
    MQTT_Disconnect(&client);
    return NULL;
  }

  /* modbus set slave id for local ctx server */
  if (modbus_set_slave(local_ctx, SERVER_SLAVE_ID) == -1)
  {
    log_error("Invalid slave ID for local");
    modbus_free(local_ctx);
    MQTT_Disconnect(&client);
    return NULL;
  }

  /* modbus set response tiemout. for local_ctx server, i set 1 seconds */
  modbus_set_response_timeout(local_ctx, 1, 0); // 1-second timeout

  time_t last_log_time = time(NULL);

  while(1) 
  {
    /* current time taken */
    time_t current_time = time(NULL);

    if (current_time - last_log_time >= 30) 
    {
      log_data("MQTT thread is active. Current time: %s", ctime(&current_time));
      last_log_time = current_time;
    }

    /* modbus connect the local ctx server */
    if (modbus_connect(local_ctx) == -1) 
    {
      log_error("Local Modbus connection failed: %s", modbus_strerror(errno));
      sleep(5);
      continue;
    }

    uint16_t reg[20];	//  reg[20] this for register buffer 

    /* modbus read register for local_ctx server data. 0 to 20 register read a data */
    int num_read = modbus_read_registers(local_ctx, 0, 20, reg);

    modbus_close(local_ctx);  //  modbus close the local_ctx server

    if (num_read == -1) 
    {
      log_error("Failed to read local registers: %s", modbus_strerror(errno));
      sleep(5);
      continue;
    }

    // server register data printing
    for(int i = 0; i < num_read; i++)
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

    // Mqtt_Publish data
    // Attempt publish regardless of connection status
    if (MQTT_Publish(&client, json_buffer, MQTT_TOPIC) != 0)
    {
      log_error("Failed to publish first sensor");
      MQTT_Disconnect(&client);		// Mqtt Disconnect for client

      if (MQTT_Connect(&client, MQTT_USERNAME, MQTT_PASSWORD) != 0)   //  when Mqtt Publish failed means  reconnect for Mqtt Connect
      {
        log_error("Failed to reconnect MQTT");
        sleep(10);
        continue;
      }
    }
    else 
    {
      printf("Publish to topic");
    }
    sleep(300);   // 5 minutes once send data in mqtt publish data
  }

  /* free to modbus local ctx */
  modbus_free(local_ctx);

  /* mqtt disconnect clien */
  MQTT_Disconnect(&client);
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

  /* read config funciton failed means return 1 */
  if (read_config_file() != 0)
  {
    log_error("Failed to read configuration files");
    perror("Failed to read configuration files");
    return 1;
  }

  // pthread declare the variables
  pthread_t co2_tid, server_tid, mqtt_tid;

  /*  pthread create for cilent */

  //  int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg);
  if (pthread_create(&co2_tid, NULL, co2_client_thread, NULL) != 0)
  {
    log_error("Failed to create CO2 client thread");
    perror("Failed to create CO2 client thread");
    return 1;
  }

  /*  pthread create for server  */
  if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0)
  {
    log_error("Failed toe create server thread");
    perror("Failed toe create server thread");
    return 1;
  }

  /*  pthread create for mqtt */
  if (pthread_create(&mqtt_tid, NULL, mqtt_thread, NULL) != 0)
  {
    log_error("Failed to create MQTT thread");
    perror("Failed to create MQTT thread");
    return 1;
  }

  //  int pthread_join(pthread_t thread, void **retval);
  pthread_join(co2_tid, NULL);		//  pthread join for client
  pthread_join(server_tid, NULL);	//  pthread join for server
  pthread_join(mqtt_tid, NULL);		//  pthread join for mqtt

#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}
