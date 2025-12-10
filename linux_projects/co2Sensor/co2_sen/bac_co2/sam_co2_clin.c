#include "server.h"
#include "co2_client.h"
#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void *co2_client_thread(void *arg)
{
  modbus_t *ctx[2] = {NULL, NULL}; // Contexts for devices 1 and 2
  modbus_t *server_ctx = NULL;

  uint16_t reg[2][NUM_OF_REGISTERS]; // Registers for devices 1 and 2
  uint16_t scaled_ppm[2][NUM_OF_REGISTERS];

  time_t last_log_time = time(NULL);
  int co2_device_indices[2] = {-1, -1};	 // Indices for devices 1 and 2
  int keep_running = 1; 		// Control flag for thread termination

  // Find CO2 devices (IDs 1 and 2)
  for (int i = 0; i < total_devices; i++) 
  {
    if (devices[i].id == 1)    // device one
    {
      co2_device_indices[0] = i;
      printf("CO2 device index 1 = %d\n", co2_device_indices[0]);
    }
    else if (devices[i].id == 2) 
    {
      co2_device_indices[1] = i;
      printf("CO2 device index 2 = %d\n", co2_device_indices[1]);
    }
  }

  if (co2_device_indices[0] == -1 || co2_device_indices[1] == -1) 
  {
    log_error("CO2 device(s) not found in configuration");
    printf("CO2 device(s) not found in configuration\n");
    fflush(stdout);
    return NULL;
  }

  // Initialize Modbus contexts for devices
  for (int i = 0; i < 2; i++) 
  {
    // modbus tcp connect 2 device sensor for 192.168.0.200, 192.168.0.201
    ctx[i] = modbus_new_tcp(devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number);   // two device.csv inside modbus 192.168.0.200, 192.168.0.201

    if (ctx[i] == NULL)
    {
      log_error("Unable to allocate libmodbus for CO2 device %d: %s", i + 1, modbus_strerror(errno));   // first time loop i + 1 data 0 + 1 = 1 device 1, second time loop i + 1 data 1 + 1 = 1 device 2
      printf("Unable to allocate libmodbus for CO2 device %d: %s\n", i + 1, modbus_strerror(errno));
      fflush(stdout);
      return NULL;
    }

    if (modbus_set_slave(ctx[i], devices[co2_device_indices[i]].device_slave_ID) == -1)   // two device.csv file modbus both device slave id  for 1 same
    {
      log_error("Invalid slave ID for CO2 device %d: %s", i + 1, modbus_strerror(errno));
      printf("Invalid slave ID for CO2 device %d: %s\n", i + 1, modbus_strerror(errno));
      fflush(stdout);
      modbus_free(ctx[i]);	// free the modbus tcp
      ctx[i] = NULL;            // set null for tcp return ctx[i]
      return NULL;
    }

    //  modus tcp connect for 2 device sensor 
    if (modbus_connect(ctx[i]) == -1) 
    {
      log_error("CO2 Modbus connection failed for device %d: %s", i + 1, modbus_strerror(errno));
      printf("CO2 Modbus connection failed for device %d: %s\n", i + 1, modbus_strerror(errno));
      fflush(stdout);
      modbus_free(ctx[i]);
      ctx[i] = NULL;
      return NULL;
    }

    // printing for co2 devices
    printf("Connected to CO2 device %d (%s:%d, slave=%d)\n",	
	i + 1, devices[co2_device_indices[i]].device_IP,
	devices[co2_device_indices[i]].device_port_number,
	devices[co2_device_indices[i]].device_slave_ID);
    fflush(stdout);
  }

  // Initialize Modbus server context
  server_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);   // modbus connect for SERVER ADDRESS ---> 192.168.0.136, SERVER PORT ---> 5503

  if (server_ctx == NULL || modbus_connect(server_ctx) == -1) 		// modbus connect server and server ctx return value set NULL
  {
    log_error("Failed to connect to Modbus server (%s:%d): %s", SERVER_ADDRESS, SERVER_PORT, modbus_strerror(errno));
    printf("Failed to connect to Modbus server (%s:%d): %s\n", SERVER_ADDRESS, SERVER_PORT, modbus_strerror(errno));
    fflush(stdout);

    for (int i = 0; i < 2; i++) 
    {
      if (ctx[i])
      {
	modbus_close(ctx[i]);   // two device also close and free 
	modbus_free(ctx[i]);
      }
    }
    return NULL;
  }

  if (modbus_set_slave(server_ctx, SERVER_SLAVE_ID) == -1)   // modbus set server for slave id
  {
    log_error("Invalid slave ID for server: %s", modbus_strerror(errno));
    printf("Invalid slave ID for server: %s\n", modbus_strerror(errno));
    fflush(stdout);

    modbus_free(server_ctx);   // modbus server free

    for (int i = 0; i < 2; i++) 
    {
      if (ctx[i]) 
      {
	modbus_close(ctx[i]);  // modbus server close
	modbus_free(ctx[i]);
      }
    }
    return NULL;
  }

  /**/

  while (keep_running)
  {
    time_t current_time = time(NULL);

    if (current_time - last_log_time >= 30) 
    {
      log_data("CO2 client thread is active. Current time: %s", ctime(&current_time));
      printf("CO2 client thread is active at %s", ctime(&current_time));
      fflush(stdout);
      last_log_time = current_time;
    }

    for (int i = 0; i < 2; i++)   two device connecting
    {
      int start_reg = (i == 0) ? CO2_REGISTER_START : CO2_REGISTER_START2;   // sensor two devices 

      int success = 0;
      int retry_count = 0;
      const int max_retries = 5;

      // Reconnect if context is NULL
      if (ctx[i] == NULL) 		// modbus failed means means reconnect 
      {
	ctx[i] = modbus_new_tcp(devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number);   // modbus tcp reconnect failed means

	if (ctx[i] == NULL || modbus_set_slave(ctx[i], devices[co2_device_indices[i]].device_slave_ID) != 0 || modbus_connect(ctx[i]) != 0)  // modbus set slave id
       	{
	  log_error("Failed to reconnect to device %d (%s:%d, slave=%d): %s",
	      i + 1, devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number,
	      devices[co2_device_indices[i]].device_slave_ID, modbus_strerror(errno));

	  printf("Failed to reconnect to device %d (%s:%d, slave=%d): %s\n",
	      i + 1, devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number,
	      devices[co2_device_indices[i]].device_slave_ID, modbus_strerror(errno));

	  fflush(stdout);
	  ctx[i] = NULL;

	  // Write zeros to server registers for failed device
	  uint16_t zero_regs[NUM_OF_REGISTERS] = {0};

	  if (modbus_write_registers(server_ctx, start_reg, NUM_OF_REGISTERS, zero_regs) == -1)    // failed means 0 zero write to registers
	  {
	    log_error("Failed to write zeros to registers %d-%d: %s", start_reg, start_reg + NUM_OF_REGISTERS - 1, modbus_strerror(errno));
	    printf("Failed to write zeros to registers %d-%d: %s\n", start_reg, start_reg + NUM_OF_REGISTERS - 1, modbus_strerror(errno));
	    fflush(stdout);
	  } 

	  else 
	  {
	    printf("Wrote zeros to registers %d-%d due to device %d failure\n", start_reg, start_reg + NUM_OF_REGISTERS - 1, i + 1);
	    fflush(stdout);
	  }
	  continue;
	}

	printf("Reconnected to device %d (%s:%d, slave=%d)\n",
	    i + 1, devices[co2_device_indices[i]].device_IP,
	    devices[co2_device_indices[i]].device_port_number,
	    devices[co2_device_indices[i]].device_slave_ID);
	fflush(stdout);
      }

      // Read registers with retries
      memset(reg[i], 0, sizeof(uint16_t) * NUM_OF_REGISTERS);  // read memset set register

      memset(scaled_ppm[i], 0, sizeof(uint16_t) * NUM_OF_REGISTERS);

      while (retry_count < max_retries && keep_running)   // count and  max, keep running
      {
	int rc = modbus_read_registers(ctx[i], 0, NUM_OF_REGISTERS, reg[i]);   // modbus number of register

	if (rc == NUM_OF_REGISTERS) 
	{
	  success = 1;
	  break;
	}

       	else
       	{
	  log_error("CO2 Modbus read failed for device %d (%s:%d, slave=%d), attempt %d/%d: %s (rc=%d)",
	      i + 1, devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number,
	      devices[co2_device_indices[i]].device_slave_ID, retry_count + 1, max_retries, modbus_strerror(errno), rc);

	  printf("CO2 Modbus read failed for device %d (%s:%d, slave=%d): %s (rc=%d)\n",
	      i + 1, devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number,
	      devices[co2_device_indices[i]].device_slave_ID, modbus_strerror(errno), rc);

	  fflush(stdout);

	  modbus_close(ctx[i]);  	// modbus close

	  ctx[i] = modbus_new_tcp(devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number);   // modbus new tcp connect devices

	  if (ctx[i] == NULL || modbus_set_slave(ctx[i], devices[co2_device_indices[i]].device_slave_ID) != 0 || modbus_connect(ctx[i]) != 0)   // checking for devices. any failed means
	  {
	    log_error("CO2 Modbus reconnect failed for device %d: %s", i + 1, modbus_strerror(errno));
	    printf("CO2 Modbus reconnect failed for device %d: %s\n", i + 1, modbus_strerror(errno));
	    fflush(stdout);
	    ctx[i] = NULL;
	    break;
	  }

	  retry_count++;
	  sleep(3);
	}
      }

      if (!success && keep_running)   // failed to read registers
      {
	log_error("Failed to read registers for device %d (%s:%d, slave=%d) after %d retries",
	    i + 1, devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number,
	    devices[co2_device_indices[i]].device_slave_ID, max_retries);

	printf("Failed to read registers for device %d (%s:%d, slave=%d) after %d retries\n",
	    i + 1, devices[co2_device_indices[i]].device_IP, devices[co2_device_indices[i]].device_port_number,
	    devices[co2_device_indices[i]].device_slave_ID, max_retries);
	fflush(stdout);

	// Write zeros to server registers
	uint16_t zero_regs[NUM_OF_REGISTERS] = {0};

	if (modbus_write_registers(server_ctx, start_reg, NUM_OF_REGISTERS, zero_regs) == -1)    // failed for server means write to 0 zero all register
	{
	  log_error("Failed to write zeros to registers %d-%d: %s", start_reg, start_reg + NUM_OF_REGISTERS - 1, modbus_strerror(errno));
	  printf("Failed to write zeros to registers %d-%d: %s\n", start_reg, start_reg + NUM_OF_REGISTERS - 1, modbus_strerror(errno));
	  fflush(stdout);
	} 

	else
       	{
	  printf("Wrote zeros to registers %d-%d due to device %d failure\n", start_reg, start_reg + NUM_OF_REGISTERS - 1, i + 1);
	  fflush(stdout);
	}

	continue;
      }

      if (success)
      {
	// Scale values
	float scale_factors[] = {2000, 5000, 100, 600, 500}; // CO2, temp, humidity, etc.

	for (int j = 0; j < NUM_OF_REGISTERS; j++) 
	{
	  float ppm = (float)reg[i][j] / 4095.0 * scale_factors[j];      // calculte register 

	  scaled_ppm[i][j] = (uint16_t)ppm;

	  printf("CO2 device %d: raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n",
	      i + 1, j + start_reg, reg[i][j], j + start_reg, ppm, j + start_reg, scaled_ppm[i][j]);

	  log_data("CO2 device %d: raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d",
	      i + 1, j + start_reg, reg[i][j], j + start_reg, ppm, j + start_reg, scaled_ppm[i][j]);
	}

	// Write to server
	printf("Writing scaled_ppm to registers %d-%d: [%d, %d, %d, %d, %d]\n",
	    start_reg, start_reg + NUM_OF_REGISTERS - 1,
	    scaled_ppm[i][0], scaled_ppm[i][1], scaled_ppm[i][2], scaled_ppm[i][3], scaled_ppm[i][4]);

	fflush(stdout);

	// modbus write register to server

	if (modbus_write_registers(server_ctx, start_reg, NUM_OF_REGISTERS, scaled_ppm[i]) == -1) 
	{

	  log_error("Failed to write to server registers %d-%d: %s", start_reg, start_reg + NUM_OF_REGISTERS - 1, modbus_strerror(errno));
	  printf("Failed to write to server registers %d-%d: %s\n", start_reg, start_reg + NUM_OF_REGISTERS - 1, modbus_strerror(errno));
	  fflush(stdout);

	  modbus_close(server_ctx);	// modbus close

	  if (modbus_connect(server_ctx) == -1) 	// modbus connect
	  {
	    log_error("Server Modbus reconnect failed: %s", modbus_strerror(errno));
	    printf("Server Modbus reconnect failed: %s\n", modbus_strerror(errno));
	    fflush(stdout);
	  }
	}
      }
    }
    sleep(10);
  }

  // Write zeros to server registers on exit
  if (server_ctx) 
  {
    uint16_t zero_regs[20] = {0};

    if (modbus_write_registers(server_ctx, 0, 20, zero_regs) == -1) 	// exit server means write to register 0 zero
    {
      log_error("Failed to write zeros to server registers on exit: %s", modbus_strerror(errno));
      printf("Failed to write zeros to server registers on exit: %s\n", modbus_strerror(errno));
      fflush(stdout);
    } 

    else
    {
      printf("Wrote zeros to server registers on CO2 client exit\n");
      fflush(stdout);
    }
  }

  // Cleanup
  for (int i = 0; i < 2; i++) 
  {
    if (ctx[i]) 
    {
      modbus_close(ctx[i]);  // after modbus client close and free
      modbus_free(ctx[i]);
    }
  }

  if (server_ctx) 
  {
    modbus_close(server_ctx);  // after modbus server close and free
    modbus_free(server_ctx);
  }

  log_data("CO2 Client Thread Exit");
  printf("CO2 Client Thread Exit\n");
  fflush(stdout);
  return NULL;
}
