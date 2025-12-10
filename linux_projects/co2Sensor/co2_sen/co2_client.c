#include "co2_client.h"
#include "config.h"
#include "server.h"

static int keep_running = 1;

void *co2_client_thread(void *arg)
{
  modbus_t *ctx[MAX_DEVICES] = {NULL};

  uint16_t reg[5], reg1[5];   // two devices for two register seperate taken
  uint16_t scaled_ppm_reg[5], scaled_ppm_reg1[5];
  float ppm;

  // current time 
  time_t last_log_time = time(NULL);

  for (int i = 0; i < total_devices; i++) 
  {
    /*  modbus tcp for client 192.168.0.200 and 192.168.0.201, port 502  */
    ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);
    if (ctx[i] == NULL)
    {
      log_error("Failed to create Modbus context for device ");
      printf("Failed to create Modbus context for device\n");
      continue;
    }

    /*  modbus for client slave id set */
    if (modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0) 
    {
      log_error("Failed to set slave ID");
      printf("Failed to set slave ID %d for device %d: %s\n", devices[i].device_slave_ID, devices[i].id, modbus_strerror(errno));
      modbus_free(ctx[i]);
      ctx[i] = NULL;
      continue;
    }

    /*  modbus connect for device 1 and device 2 */
    if (modbus_connect(ctx[i]) != 0) 
    {
      log_error("Failed to connect to device");
      printf("Failed to connect to device\n");
      modbus_free(ctx[i]);
      ctx[i] = NULL;
    }

    // set timeout for client 
    uint32_t old_response_to_sec, old_response_to_usec;

    modbus_get_response_timeout(ctx[i], &old_response_to_sec, &old_response_to_usec);
    modbus_set_response_timeout(ctx[i], 0, 1000000);  // 1000ms  

    if (i == 0)  // first device connected 
    {
      printf("Connected to client first device\n");
    }
    if (i == 1)  // second device connected 
    {
      printf("Connected to client second device\n");
    }
  }

  //  modbus server tcp IP 192.168.0.136, port 5503
  modbus_t *server_ctx = modbus_new_tcp(SERVER_ADDRESS, SERVER_PORT);   

  if (server_ctx == NULL || modbus_connect(server_ctx) != 0) 
  {
    log_error("Failed to connect to Modbus server");
    printf("Failed to connect to Modbus server\n");
    for (int i = 0; i < total_devices; i++)
    {
      if (ctx[i]) modbus_free(ctx[i]);
    }
    return NULL;
  }

  uint32_t server_old_to_sec, server_old_to_usec;

  modbus_get_response_timeout(server_ctx, &server_old_to_sec, &server_old_to_usec);
  modbus_set_response_timeout(server_ctx, 0, 1000000);  // 1000ms

  //  modbus server set slave id
  if (modbus_set_slave(server_ctx, SERVER_SLAVE_ID) == -1) 
  {
    log_error("Failed to set slave ID for server");
    printf("Failed to set slave ID for server");
    modbus_free(server_ctx);
    for (int i = 0; i < total_devices; i++) 
    {
      if (ctx[i]) modbus_free(ctx[i]);
    }
    return NULL;
  }

  while (keep_running)   //  running data
  {
    time_t current_time = time(NULL);

    if (current_time - last_log_time >= 30) 
    {
      log_data("CO2 client thread is active. Current time: %s", ctime(&current_time));
      printf("CO2 client thread is active at %s", ctime(&current_time));
      last_log_time = current_time;
    }

    //  total devices 
    for (int i = 0; i < total_devices; i++) 
    {
      //uint16_t *current_reg = (i == 0) ? reg : reg1;

      if (current_time - last_log_time >= 30) 
      {
	log_data("CO2 client thread is active. Current time: %s", ctime(&current_time));
	printf("CO2 client thread is active at %s", ctime(&current_time));
	last_log_time = current_time;
      }

      /*    total devices 
      // for (int i = 0; i < total_devices; i++) 
      // {
      uint16_t *current_reg = (i == 0) ? reg : reg1;
      uint16_t *current_scaled_ppm = (i == 0) ? scaled_ppm_reg : scaled_ppm_reg1;
      int start_reg = (i == 0) ? CO2_REGISTER_START : CO2_REGISTER_START2; */
      uint16_t *current_reg;
      uint16_t *current_scaled_ppm;
      int start_reg;

      if (i == 0)  // first device data and value send
      {
	current_reg = reg;
	current_scaled_ppm = scaled_ppm_reg;
	start_reg = CO2_REGISTER_START;
      }
      else if (i == 1)  // second device data and value send
      {
	current_reg = reg1;
	current_scaled_ppm = scaled_ppm_reg1;
	start_reg = CO2_REGISTER_START2;
      }

      int num_regs = NUM_OF_REGISTERS;  // number of registers

      //  Reconnect if context is NULL, reconnect modbus clients --> client TCP IP ===> 192.168.0.200, 192.168.0.201. Port ===> for 502
      if (ctx[i] == NULL) 
      {
	ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);

	if (ctx[i] == NULL || modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0 || modbus_connect(ctx[i]) != 0) 
	{
	  log_error("Failed to reconnect to device ");
	  printf("Failed to reconnect to device");
	  ctx[i] = NULL;

	  // when modbus failed first device all register to zero for device 1
	  if (i == 0)                         
	  { 			
	    uint16_t zero_regs[5] = {0};

	    if (modbus_write_registers(server_ctx, CO2_REGISTER_START, 5, zero_regs) == -1) 
	    {
	      log_error("Failed to write zeros to registers device 1");
	      printf("Failed to write zeros to registers device 1\n");
	    }
	    else 
	    {
	      printf("Write zeros to registers device %d failure\n", devices[i].id);
	    }
	  }

	  // when modbus failed first device all register to zero for device 2
	  if (i == 1)                         
	  { 					
	    uint16_t zero_regs[5] = {0};

	    if (modbus_write_registers(server_ctx, CO2_REGISTER_START2, 5, zero_regs) == -1) 
	    {
	      log_error("Failed to write zeros to registers device 2");
	      printf("Failed to write zeros to registers device 2\n");
	    }
	    else 
	    {
	      printf("Write zeros to registers for device %d failure\n", devices[i].id);
	    }
	  }
	  continue;
	}

	uint32_t old_response_to_sec, old_response_to_usec;
	modbus_get_response_timeout(ctx[i], &old_response_to_sec, &old_response_to_usec);
	modbus_set_response_timeout(ctx[i], 0, 1000000);  // 1000ms

	printf("Reconnected to device\n");
      }

      // current regs set 0 adnd current_sacled_ppm
      memset(current_reg, 0, sizeof(uint16_t) * num_regs);      
      memset(current_scaled_ppm, 0, sizeof(uint16_t) * num_regs);

      int success = 0;
      int retry_count = 0;
      const int max_retries = 5;

      while (retry_count < max_retries && keep_running) 
      {
	/* printf("success current_reg :%hn\n", current_reg);
	printf("success num_regs :%d\n", num_regs); */

	/* 03 Read Holding Registers (4x)  */

	//  int modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest);
	int rc = modbus_read_registers(ctx[i], 0, num_regs, current_reg);
	if (rc == num_regs) 
	{
	  success = 1;
	  break;
	}
	else if (rc == -1)
	{
	  log_error("CO2 Modbus read failed for device\n");
	  printf("CO2 Modbus read failed for device rc :%d\n", rc); 
	  modbus_close(ctx[i]);

	  // modbus reconnect tcp for clients
	  ctx[i] = modbus_new_tcp(devices[i].device_IP, devices[i].device_port_number);
	  if (ctx[i] == NULL || modbus_set_slave(ctx[i], devices[i].device_slave_ID) != 0 || modbus_connect(ctx[i]) != 0) 
	  {
	    log_error("CO2 Modbus reconnect failed for device\n"); 
	    printf("CO2 Modbus reconnect failed for device\n");
	    ctx[i] = NULL;
	    break;
	  }

	  /* modbus set time out */
	  uint32_t old_response_to_sec, old_response_to_usec;
	  modbus_get_response_timeout(ctx[i], &old_response_to_sec, &old_response_to_usec);
	  modbus_set_response_timeout(ctx[i], 0, 1000000);  // 1000ms
	  retry_count++;
	  sleep(1);  // or 3
	}
      }

      if (!success && keep_running) 
      {
	log_error("Failed to read registers for device\n");
	printf("Failed to read registers for device\n");

	// Write zeros to registers for failed device, client modbus device 1, server send to zero for first device
	if (i == 0) 
	{
	  uint16_t zero_regs[5] = {0};

	  //  write registers
	  if (modbus_write_registers(server_ctx, CO2_REGISTER_START, 5, zero_regs) == -1) 
	  {
	    log_error("Failed to write zeros to registers device 1: %s", modbus_strerror(errno));
	    printf("Failed to write zeros to registers device 1: %s\n", modbus_strerror(errno));
	    fflush(stdout);
	  } 
	  else
	  {
	    printf("Wrote zeros to registers for device %d failure\n", devices[i].id);
	    fflush(stdout);
	  }
	}

	// Write zeros to registers for failed device, client modbus device 2, server send to zero for second device
	if (i == 1) 
	{
	  uint16_t zero_regs[5] = {0};

	  //  write registers
	  if (modbus_write_registers(server_ctx, CO2_REGISTER_START2, 5, zero_regs) == -1) 
	  {
	    log_error("Failed to write zeros to registers device 2: %s", modbus_strerror(errno));
	    printf("Failed to write zeros to registers device 2: %s\n", modbus_strerror(errno));
	    fflush(stdout);
	  } 
	  else
	  {
	    printf("Wrote zeros to registers for device %d failure\n", devices[i].id);
	    fflush(stdout);
	  }
	}
      }

      // This one for main client data send server 
      if (success)
      {
	for (int j = 0; j < num_regs; j++) 
	{
	  //  this for calculate ppm formal, ppm = Rawvalue / 4095.0 * 2000
	  float scale = (j == 0 ? 2000 : j == 1 ? 5000 : j == 2 ? 100 : j == 3 ? 600 : 500);
	  ppm = (float)current_reg[j] / 4095.0 * scale;

	  current_scaled_ppm[j] = (int)ppm;   // current_sacled ppm. ppm float value convert integer value

	  /* data value to print output */
	  //  printf("\nwrite reg%s %d : %d\n", (i == 0) ? "" : "1", j, current_reg[j]);

	  /* first device for printing data */
	  if (i == 0)
	  {
	    // printf("\nfirst write reg %d : %d\n", j, current_reg[j]);
	    printf("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);
	    log_data("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);
	  }
	  else if (i == 1)  /* second device for printing data */
	  {	    
	    // printf("\nsecond write reg %d : %d\n", j, current_reg[j]);
	    printf("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);
	    log_data("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);
	  }
	  // printf("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);

	  /* log data print to log file */
	  // log_data("write reg%s %d : %d\n", (i == 0) ? "" : "1", j, current_reg[j]);
	  // log_data("CO2 : raw%d = %d, ppm%d = %.2f, scaled_ppm%d = %d\n", j + start_reg, current_reg[j], j + start_reg, ppm, j + start_reg, current_scaled_ppm[j]);
	}

	for (int j = 5; j < 8; j++)   // remaining register data send value printing 
	{
	  printf("CO2 : reg%s[%d] = %d\n", (i == 0) ? "" : "1", j, 0);
	  log_data("CO2 : reg%s[%d] = %d\n", (i == 0) ? "" : "1", j, 0);
	}

	// printf the scaled_ppm register values
	/* printf("Writing scaled_ppm to registers %d-%d: [%d, %d, %d, %d, %d]\n\n\n",
	    start_reg, start_reg + num_regs - 1,
	    current_scaled_ppm[0], current_scaled_ppm[1], current_scaled_ppm[2],
	    current_scaled_ppm[3], current_scaled_ppm[4]);  */

	/*	log_data("Writing scaled_ppm to registers %d-%d: [%d, %d, %d, %d, %d]\n\n\n",
		start_reg, start_reg + num_regs - 1,
		current_scaled_ppm[0], current_scaled_ppm[1], current_scaled_ppm[2],
		current_scaled_ppm[3], current_scaled_ppm[4]);  */

	// modbus client for two device data send to modbus write for server registers

	//   int modbus_write_register(modbus_t *ctx, int addr, const uint16_t value); 
	if (modbus_write_registers(server_ctx, start_reg, num_regs, current_scaled_ppm) == -1)   // modbus write register
	{
	  log_error("Failed to write to server registers %d-%d: %s", start_reg, start_reg + num_regs - 1, modbus_strerror(errno));
	  printf("Failed to write to server registers %d-%d: %s\n", start_reg, start_reg + num_regs - 1, modbus_strerror(errno));

	  modbus_close(server_ctx);

	  if (modbus_connect(server_ctx) == -1) 
	  {
	    log_error("Server Modbus reconnect failed: %s", modbus_strerror(errno));
	    printf("Server Modbus reconnect failed: %s\n", modbus_strerror(errno));
	  }
	}
      }
    }
    sleep(5);
    }

    // Write zeros to server registers before exiting
    if (server_ctx != NULL) 
    {
      uint16_t zero_regs[20] = {0};

      if (modbus_write_registers(server_ctx, 0, 20, zero_regs) == -1) 
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

    for (int i = 0; i < total_devices; i++) 
    {
      if (ctx[i]) 
      {
	modbus_close(ctx[i]);
	modbus_free(ctx[i]);
      }
    }
    return NULL;
}
