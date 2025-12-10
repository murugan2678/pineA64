#include "config.h"
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 	
#include <errno.h> 	
#include <string.h> 	


DeviceInfo devices[MAX_DEVICES]; 	// structure for DeviceInfo access the device
int total_devices = 0; 		// total device i set first for 0

// log directory
static int ensure_log_directory(const char *dir_path)
{
  struct stat st = {0};

  uid_t uid = getuid(); 
  gid_t gid = getgid(); 

  if (stat(dir_path, &st) == -1)
  {
    if (mkdir(dir_path, 0755) == -1)
    {
      fprintf(stderr, "Failed to create directory %s: %s\n", dir_path, strerror(errno));
      return -1;
    }
    if (chown(dir_path, uid, gid) == -1)
    {
      fprintf(stderr, "Failed to set ownership for %s: %s\n", dir_path, strerror(errno));
      return -1;
    }
  }
  else
  {
    if (chmod(dir_path, 0755) == -1)
    {
      fprintf(stderr, "Failed to set permissions for %s: %s\n", dir_path, strerror(errno));
      return -1;
    }
    if (chown(dir_path, uid, gid) == -1)
    {
      fprintf(stderr, "Failed to set ownership for %s: %s\n", dir_path, strerror(errno));
      return -1;
    }
  }
  return 0;
}

//  read config file 
int read_config_file(void)
{
  FILE *file = fopen(DEV_FILE_PATH, "r");
  if (file == NULL)
  {
    log_error("Failed to open device.csv file: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  char line[MAX_LINE_SIZE];
  char data[MAX_DEVICES][MAX_COLS][MAX_LINE_SIZE];
  int row = 0;

  char *fg = fgets(line, MAX_LINE_SIZE, file);

  if (fg == NULL)
  {
    fclose(file);
    log_error("Empty file for device.csv");
    exit(EXIT_FAILURE);
  }

  while (fgets(line, MAX_LINE_SIZE, file) && row < MAX_DEVICES)
  {
    char *token = strtok(line, ",");
    int col = 0;
    while (token && col < MAX_INFO_MAX_COL)
    {
      token[strcspn(token, "\r\n")] = 0;

      strncpy(data[row][col], token, MAX_LINE_SIZE - 1);

      data[row][col][MAX_LINE_SIZE - 1] = '\0';
      token = strtok(NULL, ",");
      col++;
    }
    if (col == MAX_INFO_MAX_COL)
    {
      row++;
    }
  }

  fclose(file);
  total_devices = row;
  printf("total devices : %d\n\n\n", total_devices);

  for (int i = 0; i < row; i++)
  {
    devices[i].id = atoi(data[i][0]);
    devices[i].device_IP = strdup(data[i][1]);
    devices[i].device_port_number = atoi(data[i][2]);
    devices[i].device_slave_ID = atoi(data[i][3]);
    devices[i].device_function_code = atoi(data[i][4]);
    devices[i].device_quantiy_to_read = atoi(data[i][5]);
  }
  return 0;
}

//  log error message
void log_error(const char *str, ...)
{
  char dir_path[256];  

  strncpy(dir_path, ERROR_LOG_FILE_PATH, sizeof(dir_path));

  dir_path[sizeof(dir_path) - 1] = '\0';

  char *last_slash = strrchr(dir_path, '/');

  if (last_slash)
  {
    *last_slash = '\0';

    if (ensure_log_directory(dir_path) == -1)
    {
      fprintf(stderr, "Cannot ensure error log directory %s\n", dir_path);
    }
  }

  FILE *file = fopen(ERROR_LOG_FILE_PATH, "a"); 
  if (file == NULL)
  {
    fprintf(stderr, "cannot open the error log file %s: %s\n", ERROR_LOG_FILE_PATH, strerror(errno));
    return;
  }

  va_list args;
  va_start(args, str);

  char buffer[1024];  

  vsnprintf(buffer, sizeof(buffer), str, args);

  va_end(args);

  time_t now = time(NULL);

  char *time_str = ctime(&now);
  time_str[strcspn(time_str, "\n")] = '\0';
  fprintf(file, "[%s] %s\n", time_str, buffer);

  if (fchmod(fileno(file), 0644) == -1)
  {
    fprintf(stderr, "Failed to set permissions for %s: %s\n", ERROR_LOG_FILE_PATH, strerror(errno));
  }

  if (fchown(fileno(file), getuid(), getgid()) == -1)
  {
    fprintf(stderr, "Failed to set ownership for %s: %s\n", ERROR_LOG_FILE_PATH, strerror(errno));
  }
  fclose(file);
}

//  log data message 
void log_data(const char *str, ...)
{
  char filename[256];

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  snprintf(filename, sizeof(filename), "%s%04d-%02d-%02d.txt", LOG_FILE_PATH, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

  char dir_path[256];

  strncpy(dir_path, LOG_FILE_PATH, sizeof(dir_path));
  dir_path[sizeof(dir_path) - 1] = '\0';

  size_t len = strlen(dir_path);
  if (len > 0 && dir_path[len - 1] == '/')
  {
    dir_path[len - 1] = '\0';
  }

  if (ensure_log_directory(dir_path) == -1)
  {
    fprintf(stderr, "Cannot ensure log directory %s\n", dir_path);

  }

  FILE *file = fopen(filename, "a");
  if (file == NULL)
  {
    fprintf(stderr, "cannot open the log file %s: %s\n", filename, strerror(errno));
    return;
  }

  va_list args;
  va_start(args, str);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), str, args);
  va_end(args);

  char *str_tm = ctime(&now);
  str_tm[strcspn(str_tm, "\n")] = '\0';
  fprintf(file, "[%s] %s\n", str_tm, buffer);

  if (fchmod(fileno(file), 0644) == -1)
  {
    fprintf(stderr, "Failed to set permissions for %s: %s\n", filename, strerror(errno));
  }

  if (fchown(fileno(file), getuid(), getgid()) == -1)
  {
    fprintf(stderr, "Failed to set ownership for %s: %s\n", filename, strerror(errno));
  }
  fclose(file);
} 
