/************************************************************************************************************************************************************************/  
/*    Author : Murugan M																		*/
/*    Data   : 20-2-2026																	        */
/*																				        */	
/*    Program Description																		*/
/*																				        */
/*	The Pine A64 HAT board uses UART2 (/dev/ttyS2) as a 													        */
/*	Modbus RTU master over RS485 (manual DE/RE GPIO control) to poll and read all 18 parameter registers 								*/
/*	from the VAV thermostat at a configurable interval. All 18 parameters are published via MQTT to Zedbee cloud for full BMS telemetry.				*/
/*	Bidirectional MQTT read/write, GET commands, and direct Modbus register mapping are supported exclusively for the four key parameters: VAS, MOD, ST2, and SPS.	*/
/*																					*/
/************************************************************************************************************************************************************************/




#define _DEFAULT_SOURCE     //  this mqtt_yield function mentions

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <modbus/modbus.h>
#include <gpiod.h>
#include <MQTTAsync.h>

// this are header function inside there  
#include "data.h"
#include "config.h"
#include "gpio_control.h"
#include "mqtt.h"
#include "jsonify.h"


#include <unistd.h>

extern time_t publish_interval_sec;   // this for extern variable time example ---> 120, 180

// Reconnect timing
static time_t last_reconnect_attempt = 0;
static const time_t reconnect_delay_sec = 30;  // try reconnect every 30sec

//  Alive log timing
static time_t last_alive_log = 0;
static const time_t alive_log_interval_sec = 60;

// Periodic forward timing
static time_t last_forward_attempt = 0;
static const time_t forward_interval_sec = 120;

// this for print output data
void print_register(int idx, uint16_t val) // idx means ---> index value ---> like 0,1,2,3,4,5. val this only vav data come
{
  //  this for switch case. i use vav register 18 register. one by one vav register data print output 
  switch (idx) 
  {
    case 0:  
      printf(" %2d | %5u | Device ID\n", idx, val); 
      break;

    case 1:  
      printf(" %2d | %5u | VAS (fan)\n", idx, val); 
      break;

    case 3:  
      printf(" %2d | %5u | MOD (0=auto,1=man)\n", idx, val); 
      break;

    case 6:  
      printf(" %2d | %5.1f | Vnom\n", idx, val/1.0f); 
      break;

    case 7:  
      printf(" %2d | %5.1f | Vmin\n", idx, val/1.0f); 
      break;

    case 8:  
      printf(" %2d | %5.1f | Vmax\n", idx, val/1.0f); 
      break;

    case 9:  
      printf(" %2d | %5.2f | DMX deltaP\n", idx, val/100.0f); 
      break;

    case 11:
      printf(" %2d | %5.2f | sT2 setpoint °C\n", idx, val/100.0f); 
      break;

    case 13: 
      printf(" %2d | %5u | PIR\n", idx, val); 
      break;

    case 14: 
      printf(" %2d | %5.2f | OFS offset °C\n", idx, val/100.0f); 
      break;

    case 15: 
      printf(" %2d | %5u | PFC\n", idx, val); 
      break;

    case 16: 
      printf(" %2d | %5u | dir (0=fwd)\n", idx, val); 
      break;

    case 17: 
      printf(" %2d | %5.1f | SPS damper %%\n", idx, val/1.0f); 
      break;

    default: 
      printf(" %2d | %5u |\n", idx, val);
      break;
  }
}

//  this for main function here started point progam executing
int main(void) 
{
  printf("VAV Thermostat starting ...\n\n");

  //  modbus_t *modbus_new_rtu(const char *device, int baud, char parity, int data_bit, int stop_bit);
  modbus_t *mb = modbus_new_rtu(DEVICE, BAUD_RATE, PARITY, DATA_BITS, STOP_BITS);
  if (!mb) 
  {
    perror("modbus_new_rtu");
    return 1;
  }

  //  this for gpiochip initializing funtion. call to here 
  init_gpio();

  //  this publish interval function this only deside this time
  load_publish_interval();

  //  debug the print
  printf("Modbus: %s @ %d baud slave=%d\n", DEVICE, BAUD_RATE, SLAVE_ID);
  printf("MQTT:   %s client=%s\n", BOODSKAP_BROKER, CLIENTID);
  printf("Topics: sub=%s  pub=%s\n", BOODSKAP_SUBSCRIBE_TOPIC, BOODSKAP_PUBLISH_TOPIC);

  //  int modbus_set_slave(modbus_t *ctx, int slave);
  modbus_set_slave(mb, SLAVE_ID);

  //  int modbus_set_debug(modbus_t *ctx, int flag);
  modbus_set_debug(mb, 1);

  //  int modbus_set_response_timeout(modbus_t *ctx, uint32_t to_sec, uint32_t to_usec);
  modbus_set_response_timeout(mb, 2, 0);

  //  void modbus_set_byte_timeout(modbus_t *ctx, uint32_t to_sec, uint32_t to_usec);
  modbus_set_byte_timeout(mb, 0, 500000);

  //  int modbus_rtu_set_rts(modbus_t *ctx, int mode)
  modbus_rtu_set_rts(mb, MODBUS_RTU_RTS_UP);

  //  int modbus_rtu_set_custom_rts(modbus_t *ctx, void (set_rts) (modbus_t ctx, int on))
  modbus_rtu_set_custom_rts(mb, rts_control);

  //  int modbus_connect(modbus_t *ctx);
  if (modbus_connect(mb) == -1) 
  {
    perror("modbus_connect");

    // void modbus_free(modbus_t *ctx);
    modbus_free(mb);
    return 1;
  }

  //  this one access to modbus new rtu fd
  cmd_ctx.modbus_ctx = mb;

  //  Mqtt asynchrouns declare the variable for mqtt
  MQTTAsync mqtt;

  //  Mqtt initilezing for mqtt setup this function for mqtt.c file
  MQTT_Init(&mqtt);

  //  this one acces to mqtt address. take for cmd ---> commands 
  cmd_ctx.mqtt_client = &mqtt;

  //  this one mqtt connect or not check this function
  if (MQTT_Connect(&mqtt, BOODSKAP_USERNAME, BOODSKAP_PASSWORD) != 0) 
  {
    printf("[MQTT] Connect request sent (async)\n");
  }

  // this for vav thermostat structure access declare the variable for sensor
  VavSensorData sensor = {0};

  // time variable declare for last pub
  time_t last_pub = 0;

  // continuesly run. mqtt failed, modbus read failed, modbus connect faild. faild come not exit program. failed also come continues running progam
  while (1) 
  {
    // MQTTAsync_yield(&mqtt, 100);  // WRONG
    //      MQTT_Yield(&mqtt, 100);  // CORRECT - use your wrapper

    //      MQTTAsync_receive(&mqtt, 500);   // timeout in milliseconds

    //  get time a seconds
    //  time_t time(time_t *tloc);
    time_t now = time(NULL);

    //  Periodic alive log
    //  current time minus last alive log compare to alive log interval seconds
    if (now - last_alive_log >= alive_log_interval_sec) 
    {
      // declare the character timestr buffer
      char timestr[32];

      //  this function for format date and time
      //  size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
      strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));

      printf("[ALIVE] Program running | MQTT: %s | %s\n", mqtt_is_connected ? "connected" : "DISCONNECTED", timestr);

      //  current time passed to last alive log
      last_alive_log = now;
    }

    // Automatic reconnect attempt
    if (!mqtt_is_connected && (now - last_reconnect_attempt >= reconnect_delay_sec))
    {
      printf("[MQTT] Disconnected → trying to reconnect...\n");

      //  mqtt reconnected logic
      int rc = MQTT_Connect(&mqtt, BOODSKAP_USERNAME, BOODSKAP_PASSWORD);
      if (rc == 0)
      {
	printf("[MQTT] Reconnect request queued\n");
      }
      else
      {
	printf("[MQTT] Reconnect failed immediately (rc=%d) – retry later\n", rc);
      }
      last_reconnect_attempt = now;

    }

    // Try to forward stored messages periodically when connected
    if (mqtt_is_connected && (now - last_forward_attempt >= forward_interval_sec)) 
    {
      printf("[OFFLINE] Trying to forward stored messages...\n");

      // try to forward message
      forward_stored_messages(&mqtt);
      last_forward_attempt = now;
    }


    usleep(800000);

    uint16_t regs[NUM_REGS] = {0};

    //  holding registers ---> function 3 in modbus poll

    //  int modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest);
    int n = modbus_read_registers(mb, 0, NUM_REGS, regs);
    if (n != NUM_REGS) 
    {
      perror("modbus_read_registers");
      usleep(500000);
      continue;
    }

    // Fill sensor data parameter registers access in structure using VavSensorData ---> sensor
    sensor.device_id          = regs[0];
    sensor.vav_status         = regs[1];
    sensor.v2s_source         = regs[2];
    sensor.mode               = regs[3];
    sensor.cfm                = regs[4];
    sensor.ccf                = regs[5];
    sensor.vnm                = regs[6];
    sensor.vmn                = regs[7];
    sensor.vmx                = regs[8];
    sensor.dmx                = regs[9];
    sensor.dmp                = regs[10];
    sensor.set_temp           = regs[11];
    sensor.amb_temp           = regs[12];
    sensor.pir_status         = regs[13];
    sensor.amb_temp_offset    = regs[14];
    sensor.pfc_control        = regs[15];
    sensor.direction          = regs[16];
    sensor.set_damper_position = regs[17];

    printf("\n--- Poll ---\n");
    for (int i = 0; i < NUM_REGS; i++)
    {
      //  this function for print register output
      print_register(i, regs[i]);
    }

    printf("Current MQTT publish interval: %ld seconds | MQTT: %s\n", publish_interval_sec, mqtt_is_connected ? "connected" : "offline");

    /* if (now - last_pub >= publish_interval_sec) 
       {
       char buf[BUFFER_SIZE1];
       createVavJson(buf, &sensor, 1, now);

       printf("\n *****main mqtt data publish ##### Publishing (interval %ld s):\n%s\n", publish_interval_sec, buf);

       if (MQTT_Publish(&mqtt, BOODSKAP_PUBLISH_TOPIC, buf) == 0) 
       {
       printf("***** PUBLISH SUCCESS *****\n");
       last_pub = now;
       }
       } */

    //  this for mqtt data send time 5 minutes once send like ---> 300
    if (now - last_pub >= publish_interval_sec)
    {
      char buf[BUFFER_SIZE1];

      //  this function for data send this function create json formate
      createVavJson(buf, &sensor, 1, now);

      printf("\n *****main mqtt data publish ##### Publishing (interval %ld s):\n%s\n", publish_interval_sec, buf);

      //  this only main publish data for 5 minuted once send to publish zedbee cloude bms
      if (MQTT_Publish(&mqtt, BOODSKAP_PUBLISH_TOPIC, buf) == 0)
      {
	printf("***** PUBLISH SUCCESS *****\n");
      }  // No else needed - always update timer
      last_pub = now;  // ← ADD THIS LINE: Reset timer every attempt
    }


    usleep(4200000);  // ≈ 4.2 seconds loop
  }

  //  Cleanup (unreachable in current infinite loop)

  //  void modbus_close(modbus_t *ctx);
  modbus_close(mb);

  //  void modbus_free(modbus_t *ctx);
  modbus_free(mb);

  MQTT_Disconnect(&mqtt);
  MQTTAsync_destroy(&mqtt);

  return 0;
}
