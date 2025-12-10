#include "server.h"
#include "co2_client.h"
#include <modbus/modbus.h>

void *co2_client_thread(void *arg)
{
  modbus_t *ctx = NULL;
  modbus_t *ctx1 = NULL;
  modbus_t *server_ctx = NULL;

  uint16_t reg[NUM_OF_REGISTERS];
  uint16_t reg1[NUM_OF_REGISTERS];

  time_t last_log_time = time(NULL);

  int co2_device_index = -1;
  int co2_device_index1 = -1;

  // Find CO2 device (ID 1)
  for(int i = 0; i < total_devices; i++)
  {
    if(devices[i].id == 1)
    {
      co2_device_index = i;
      printf("co2 device index 1 = %d\n", co2_device_index);
      continue;
    }
    if(devices[i].id == 2)
    {
      co2_device_index1 = i;
      printf("co2 device index 2 = %d\n", co2_device_index1);
      break;
    }
  }

  if (co2_device_index == -1)
  {
    log_error("CO2 device not found in configuration for index 1");
    return NULL;
  }
  if (co2_device_index1 == -1)
  {
    log_error("CO2 device not found in configuration for index 2");
    return NULL;
  }

  // modbus connect for first device ip ---> 192.168.0.200
  ctx = modbus_new_tcp(devices[co2_device_index].device_IP, devices[co2_device_index].device_port_number);
  if (ctx == NULL)
  {
    log_error("Unable to allocate libmodbus for CO2 index 1");
    perror("Unable to allocate libmodbus for CO2 index 1");
    return NULL;
  }

  // modbus  connect for second sesnor ip ---> 192.168.0.201
  ctx1 = modbus_new_tcp(devices[co2_device_index1].device_IP, devices[co2_device_index1].device_port_number);
  if (ctx1 == NULL)
  {
    log_error("Unable to allocate libmodbus for CO2 index 2");
    perror("Unable to allocate libmodbus for CO2 index 2");
    modbus_free(ctx);
    return NULL;
  }

  // modbus set slave id for 1. first device 
  int sl = modbus_set_slave(ctx, devices[co2_device_index].device_slave_ID);
  if (sl == -1)
  {
    log_error("Invalid slave ID for CO2 : %s index 1", modbus_strerror(errno));
    perror("Invalid slave ID for CO2 index 1");
    modbus_free(ctx);
    return NULL;
  }

  // modbus set slave id for 1. second device 
  int sl1 = modbus_set_slave(ctx1, devices[co2_device_index1].device_slave_ID);
  if (sl1 == -1)
  {
    log_error("Invalid slave ID for CO2 index 2: %s", modbus_strerror(errno));
    perror("Invalid slave ID for CO2 index 2");
    modbus_free(ctx);
    modbus_free(ctx1);
    return NULL;
  }

  // modbus connect first device
  int modCon = modbus_connect(ctx);
  if (modCon == -1)
  {
    log_error("CO2 Modbus connection failed index 1: %s", modbus_strerror(errno));
    perror("CO2 Modbus connection failed index 1");
    modbus_free(ctx);
    return NULL;
  }

  // modbus connect se device
  int modCon = modbus_connect(ctx);
  int modCon1 = modbus_connect(ctx1);
  if (modCon1 == -1)
  {
    log_error("CO2 Modbus connection failed index 2: %s", modbus_strerror(errno));
    perror("CO2 Modbus connection failed index 2");
    modbus_free(ctx);
    modbus_free(ctx1);
    return NULL;
  }

  //  modbus server IP 192.168.0.136, PORT 5503
  server_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);    // modbus server connect
  if (server_ctx == NULL)
  {
    log_error("Unable to allocate libmodbus for server : %s", modbus_strerror(errno));
    modbus_close(ctx);
    modbus_free(ctx);
    modbus_close(ctx1);
    modbus_free(ctx1);
    return NULL;
  }

  //  modbus server set slave id
  if (modbus_set_slave(server_ctx, SERVER_SLAVE_ID) == -1)
  {
    log_error("Invalid Slave ID for server : %s", modbus_strerror(errno));
    modbus_free(server_ctx);
    modbus_close(ctx);
    modbus_free(ctx);
    modbus_close(ctx1);
    modbus_free(ctx1);
    return NULL;
  }

  // modbus connect server
  if (modbus_connect(server_ctx) == -1)
  {
    log_error("Server Modbus connection failed : %s", modbus_strerror(errno));
    modbus_free(server_ctx);
    modbus_close(ctx);
    modbus_free(ctx);
    modbus_close(ctx1);
    modbus_free(ctx1);
    return NULL;
  }

  //  int notcnt1, notcnt2;
  while(1)
  {
    time_t current_time = time(NULL);
    if (current_time - last_log_time >= 30)
    {
      log_data("CO2 client thread is active. Current time: %s", ctime(&current_time));
      last_log_time = current_time;
    }

  //  for (int i  = 0; i < total_devices; i++)
    //{

      // first sensor 
      uint16_t write_regs[NUM_OF_REGISTERS];
      memset(write_regs, 0, sizeof(write_regs)); // Initialize to zero, using memset funciton 

      // modbus read register for first sensor data
      int num_read = modbus_read_registers(ctx, 0, NUM_OF_REGISTERS, reg);

      if (num_read == -1)
      {
	log_error("CO2 Modbus read registers failed index 1: %s", modbus_strerror(errno));
	printf("CO2 Modbus read registers failed index 2: %s", modbus_strerror(errno));
	modbus_close(ctx);

	// Write zeros to server for index 1. first modbus device
	if (modbus_write_registers(server_ctx, CO2_REGISTER_START, NUM_OF_REGISTERS, write_regs) == -1)
	{
	  log_error("Failed to write the zerod registers to server --> index 1: %s", modbus_strerror(errno));
	  printf("Failed to write the zerod registers to server --> index 1: %s", modbus_strerror(errno));

	}
	if (modbus_connect(ctx) == -1)
	{
	  log_error("CO2 Modbus reconnect failed");
	  perror("CO2 Modbus reconnect failed");
	  sleep(10);
	  continue;
	}
	sleep(10);
	continue;
      }
      else if ( num_read > 0)
      {
	// first modbus ip for 192.168.0.200, this for first sensor
	float raw = reg[0];
	float ppm = (raw / 4095) * 2000;
	uint16_t ppm_scaled = (uint16_t)ppm;

	float raw1 = reg[1];
	float ppm1 = (raw1 / 4095) * 5000; // temperature
	uint16_t ppm_scaled1 = (uint16_t)ppm1;

	float raw2 = reg[2];
	float ppm2 = (raw2 / 4095) * 100; // hummunity
	uint16_t ppm_scaled2 = (uint16_t)ppm2;

	float raw3 = reg[3];
	float ppm3 = (raw3 / 4095) * 600;
	uint16_t ppm_scaled3 = (uint16_t)ppm3;

	float raw4 = reg[4];
	float ppm4 = (raw4 / 4095) * 500;
	uint16_t ppm_scaled4 = (uint16_t)ppm4;

	//  uint16_t write_regs[NUM_OF_REGISTERS];
	write_regs[0] = ppm_scaled;
	write_regs[1] = ppm_scaled1;
	write_regs[2] = ppm_scaled2;
	write_regs[3] = ppm_scaled3;
	write_regs[4] = ppm_scaled4;

	printf("write reg 0 : %d\n\n", write_regs[0]);
	printf("write reg 1 : %d\n\n", write_regs[1]);
	printf("write reg 2 : %d\n\n", write_regs[2]);
	printf("write reg 3 : %d\n\n", write_regs[3]);
	printf("write reg 4 : %d\n\n", write_regs[4]);

	for (int i = 5; i < NUM_OF_REGISTERS; i++)
	{
	  write_regs[i] = reg[i];
	}

	// modbus first device data. write to server  
	int rc = modbus_write_registers(server_ctx, CO2_REGISTER_START, NUM_OF_REGISTERS, write_regs);
	if (rc == -1)
	{
	  log_error("Failed to write to server registers : %s", modbus_strerror(errno));
	  modbus_close(server_ctx);
	  if(modbus_connect(server_ctx) == -1)
	  {
	    log_error("Server Modbus reconnect failed : %s", modbus_strerror(errno));
	    sleep(10);
	    continue;
	  }
	  sleep(10);
	  continue;
	}

	log_data("CO2 (index 1): raw = %d, ppm = %.2f, scaled_ppm = %d, write to server register[%d]", (int)raw, ppm, ppm_scaled, CO2_REGISTER_START);
	printf("CO2 : raw0 = %d, ppm0 = %.2f, scaled_ppm0 = %d\n", (int)raw, ppm, ppm_scaled);
	printf("CO2 : raw1 = %d, ppm1 = %.2f, scaled_ppm1 = %d\n", (int)raw1, ppm1, ppm_scaled1);
	printf("CO2 : raw2 = %d, ppm2 = %.2f, scaled_ppm2 = %d\n", (int)raw2, ppm2, ppm_scaled2);
	printf("CO2 : raw3 = %d, ppm3 = %.2f, scaled_ppm3 = %d\n", (int)raw3, ppm3, ppm_scaled3);
	printf("CO2 : raw4 = %d, ppm4 = %.2f, scaled_ppm4 = %d\n", (int)raw4, ppm4, ppm_scaled4);

	for (int i = 5; i < num_read; i++)
	{
	  log_data("CO2 : reg[%d] = %d\n", i, reg[i]);
	  printf("CO2 : reg[%d] = %d\n", i, reg[i]);
	}
      }

      // Read and process second sensor (ID 2)
      uint16_t write_regs1[NUM_OF_REGISTERS];
      memset(write_regs1, 0, sizeof(write_regs1)); // Initialize to zero

      // modbus read register for second sensor data
      int num_read1 = modbus_read_registers(ctx1, 0, NUM_OF_REGISTERS, reg1);

      if (num_read1 == -1)
      {
	log_error("CO2 Modbus read registers failed index 2 : %s", modbus_strerror(errno));
	printf("CO2 Modbus read registers failed index 2 : %s", modbus_strerror(errno));
	modbus_close(ctx1);

	if(modbus_write_registers(server_ctx, CO2_REGISTER_START2, NUM_OF_REGISTERS, write_regs1) == -1)
	{
	  log_error("Failed to write zered registers to server index 2 :%s", modbus_sterror(errno));
	  printf("Failed to write zered registers to server index 2 :%s", modbus_sterror(errno));
	}

	if (modbus_connect(ctx1) == -1)
	{
	  log_error("CO2 Modbus reconnect failed");
	  perror("CO2 Modbus reconnect failed");
	  sleep(10);
	  continue;
	}

	modbus_write_registers(server_ctx, CO2_REGISTER_START2, NUM_OF_REGISTERS, write_regs1);
	sleep(10);
	continue;
      }

      else if (num_read1 > 0)
      {
	// second modbus ip for 192.168.0.201, this for second sensor
	float raw6 = reg1[0];
	float ppm6 = (raw6 / 4095) * 2000;
	uint16_t ppm_scaled6 = (uint16_t)ppm6;

	float raw7 = reg1[1];
	float ppm7 = (raw7 / 4095) * 5000; // temperature
	uint16_t ppm_scaled7 = (uint16_t)ppm7;

	float raw8 = reg1[2];
	float ppm8 = (raw8 / 4095) * 100; // humunity
	uint16_t ppm_scaled8 = (uint16_t)ppm8;

	float raw9 = reg1[3];
	float ppm9 = (raw9 / 4095) * 600;
	uint16_t ppm_scaled9 = (uint16_t)ppm9;

	float raw10 = reg1[4];
	float ppm10 = (raw10 / 4095) * 500;
	uint16_t ppm_scaled10 = (uint16_t)ppm10;

	// uint16_t write_regs1[NUM_OF_REGISTERS];
	write_regs1[0] = ppm_scaled6;
	write_regs1[1] = ppm_scaled7;
	write_regs1[2] = ppm_scaled8;
	write_regs1[3] = ppm_scaled9;
	write_regs1[4] = ppm_scaled10;

	printf("write reg1 0 : %d\n\n", write_regs1[0]);
	printf("write reg1 1 : %d\n\n", write_regs1[1]);
	printf("write reg1 2 : %d\n\n", write_regs1[2]);
	printf("write reg1 3 : %d\n\n", write_regs1[3]);
	printf("write reg1 4 : %d\n\n", write_regs1[4]);

	for (int i = 5; i < NUM_OF_REGISTERS; i++)
	{
	  write_regs1[i] = reg1[i];
	}

	// modbus second device data. write to server  
	int rc1 = modbus_write_registers(server_ctx, CO2_REGISTER_START2, NUM_OF_REGISTERS, write_regs1);
	if (rc1 == -1)
	{
	  log_error("Failed to write to server registers (index 2) : %s", modbus_strerror(errno));
	  modbus_close(server_ctx);
	  if(modbus_connect(server_ctx) == -1)
	  {
	    log_error("Server Modbus reconnect failed : %s", modbus_strerror(errno));
	    sleep(10);
	    continue;
	  }
	  sleep(10);
	  continue;
	}

	log_data("CO2 (index 2): raw = %d, ppm = %.2f, scaled_ppm = %d, write to server register[%d]", (int)raw6, ppm6, ppm_scaled6, CO2_REGISTER_START2);


	printf("\n\n");
	printf("CO2 : raw6 = %d, ppm6 = %.2f, scaled_ppm6 = %d\n", (int)raw6, ppm6, ppm_scaled6);
	printf("CO2 : raw7 = %d, ppm7 = %.2f, scaled_ppm7 = %d\n", (int)raw7, ppm7, ppm_scaled7);
	printf("CO2 : raw8 = %d, ppm8 = %.2f, scaled_ppm8 = %d\n", (int)raw8, ppm8, ppm_scaled8);
	printf("CO2 : raw9 = %d, ppm9 = %.2f, scaled_ppm9 = %d\n", (int)raw9, ppm9, ppm_scaled9);
	printf("CO2 : raw10 = %d, ppm10 = %.2f, scaled_ppm10 = %d\n", (int)raw10, ppm10, ppm_scaled10);

	for (int i = 5; i < num_read1; i++)
	{
	  log_data("CO2 : reg1[%d] = %d\n", i, reg1[i]);
	  printf("CO2 : reg1[%d] = %d\n", i, reg1[i]);
	}
	printf("\n\n");
      }
      sleep(5);
    }
    modbus_close(ctx);
    modbus_free(ctx);
    modbus_close(ctx1);
    modbus_free(ctx1);
    modbus_close(server_ctx);
    modbus_free(server_ctx);
    log_data("CO2 Client Thread Exit");
    return NULL;
  }
}
