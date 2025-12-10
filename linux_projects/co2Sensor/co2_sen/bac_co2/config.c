#include "config.h"	// config.h this for header files
#include <sys/stat.h> 	// NEW: For mkdir, chmod
#include <sys/types.h> 	// NEW: For chown
#include <unistd.h> 	// NEW: For getuid, getgid
#include <errno.h> 	// For errno
#include <string.h> 	// For strerror

// adding new


DeviceInfo devices[MAX_DEVICES]; 	// structure for DeviceInfo access the device

int total_devices = 0; 		// total device i set first for 0

//modbus_mapping_t *mb_mapping = NULL; 	// modbus mapping for first set NULL


// NEW: Function to ensure directory exists and has correct permissions
static int ensure_log_directory(const char *dir_path)
{
  struct stat st = {0};

  /* get user identity */
  // uid_t getuid(void);
  uid_t uid = getuid(); // Get current user ID (zedbee)

  /* get group identity */
  // gid_t getgid(void);
  gid_t gid = getgid(); // Get current group ID (zedbee)

  /* Check if directory exists */
  // stat - file status  
  if (stat(dir_path, &st) == -1)
  {
    /* Create directory with 0755 (rwxr-xr-x) */
    // int mkdir(const char *pathname, mode_t mode);
    if (mkdir(dir_path, 0755) == -1)
    {
      fprintf(stderr, "Failed to create directory %s: %s\n", dir_path, strerror(errno));
      return -1;
    }
    /* Set ownership to zedbee:zedbee */
    // int chown(const char *pathname, uid_t owner, gid_t group); 
    if (chown(dir_path, uid, gid) == -1)
    {
      fprintf(stderr, "Failed to set ownership for %s: %s\n", dir_path, strerror(errno));
      return -1;
    }
  }
  else
  {
    /* Directory exists, ensure permissions */
    //  int chmod(const char *pathname, mode_t mode); 
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

int read_config_file(void)
{
  //  FILE *fopen(const char *restrict pathname, const char *restrict mode);
  FILE *file = fopen(DEV_FILE_PATH, "r");
  if (file == NULL)
  {
    log_error("Failed to open device.csv file: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* line buffer size for MAX LINE SIZE */
  char line[MAX_LINE_SIZE];
  /* data array */
  char data[MAX_DEVICES][MAX_COLS][MAX_LINE_SIZE];
  int row = 0;

  /* fgets ---> input of characters and strings */
  //  char *fgets(char s[restrict .size], int size, FILE *restrict stream);
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

      // char *strncpy(char dst[restrict .sz], const char *restrict src, size_t sz);
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
    /* devices access structure for device id, device ip, port, slave id, function code, quantity how much read quantity */
    /* atoi ---> convert a string to an integer */
    //  int atoi(const char *nptr);
    devices[i].id = atoi(data[i][0]);
    devices[i].device_IP = strdup(data[i][1]);
    devices[i].device_port_number = atoi(data[i][2]);
    devices[i].device_slave_ID = atoi(data[i][3]);
    devices[i].device_function_code = atoi(data[i][4]);
    devices[i].device_quantiy_to_read = atoi(data[i][5]);
  }

  // correction command it

  /*mb_mapping = modbus_mapping_new_start_address(0, 0, 0, 0, 0, MAX_REGISTERS, 0, 0);
    if (mb_mapping == NULL)
    {
    log_error("Failed to allocate Modbus mapping: %s", modbus_strerror(errno));
    return -1;
    }  */
  return 0;
}

void log_error(const char *str, ...)
{
  // NEW: Extract directory from ERROR_LOG_FILE_PATH
  char dir_path[256];  // dir path buffer size for 256 

  //  char *strncpy(char dst[restrict .sz], const char *restrict src, size_t sz);
  strncpy(dir_path, ERROR_LOG_FILE_PATH, sizeof(dir_path));

  dir_path[sizeof(dir_path) - 1] = '\0';

  /* strrchr ---> locate character in string */
  //  char *strrchr(const char *s, int c);
  char *last_slash = strrchr(dir_path, '/');

  if (last_slash)
  {
    *last_slash = '\0'; // Get directory part (e.g., "log")

    // ensure log directory function call to here checking 
    if (ensure_log_directory(dir_path) == -1)
    {
      //  int fprintf(FILE *restrict stream, const char *restrict format, ...);
      fprintf(stderr, "Cannot ensure error log directory %s\n", dir_path);
    }
  }

  //   FILE *fopen(const char *restrict pathname, const char *restrict mode);
  FILE *file = fopen(ERROR_LOG_FILE_PATH, "a"); // FIXED: Use ERROR_LOG_FILE_PATH
  if (file == NULL)
  {
    fprintf(stderr, "cannot open the error log file %s: %s\n", ERROR_LOG_FILE_PATH, strerror(errno));
    return;
  }

  va_list args;

  /* va_start ---> variable argument lists, variadic function */
  //  void va_start(va_list ap, last);
  va_start(args, str);

  char buffer[1024];  //  char buffer size for 1024

  //  int vsnprintf(char str[restrict .size], size_t size, const char *restrict format, va_list ap);
  vsnprintf(buffer, sizeof(buffer), str, args);

  //  void va_end(va_list ap);
  va_end(args);

  /* time ---> get time in seconds */
  //  time_t time(time_t *_Nullable tloc);
  time_t now = time(NULL);

  /* ctime ---> transform date and time to broken-down time or ASCII */
  //  char *ctime(const time_t *timep);
  char *time_str = ctime(&now);
  time_str[strcspn(time_str, "\n")] = '\0';
  fprintf(file, "[%s] %s\n", time_str, buffer);

  // NEW: Set file permissions to 0644 (rw-r--r--)
  //  int chmod(const char *pathname, mode_t mode); 
  if (fchmod(fileno(file), 0644) == -1)
  {
    //  int fprintf(FILE *restrict stream, const char *restrict format, ...);
    fprintf(stderr, "Failed to set permissions for %s: %s\n", ERROR_LOG_FILE_PATH, strerror(errno));
  }

  // NEW: Set ownership to zedbee:zedbee
  if (fchown(fileno(file), getuid(), getgid()) == -1)
  {
    fprintf(stderr, "Failed to set ownership for %s: %s\n", ERROR_LOG_FILE_PATH, strerror(errno));
  }
  fclose(file);
}

// Log data to daily log file
/*void log_data(const char *str, ...)
  {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char filename[256];
  snprintf(filename, sizeof(filename), "%s%04d-%02d-%02d.txt", LOG_FILE_PATH, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

// Ensure the log directory exists
char dir_path[512];
snprintf(dir_path, sizeof(dir_path), "%s", LOG_FILE_PATH);
char *last_slash = strrchr(dir_path, '/');

if (last_slash && last_slash != dir_path) 
{
 *last_slash = '\0'; // Terminate at the last slash to get directory path
 if (mkdir(dir_path, 0755) == -1 && errno != EEXIST) {
 fprintf(stderr, "Failed to create log directory %s: %s\n", dir_path, strerror(errno));
 return;
 }
 }

 FILE *fp = fopen(filename, "a");
 if (fp == NULL)
 {
 fprintf(stderr, "Failed to open log file %s: %s\n", filename, strerror(errno));
 return;
 }
 char *time_str = ctime(&now);
 time_str[strlen(time_str) - 1] = '\0';
 fprintf(fp, "[%s] ", time_str);
 va_list args;
 va_start(args, str);
 vfprintf(fp, str, args);
 va_end(args);
 fprintf(fp, "\n");
 fclose(fp);
 } */

void log_data(const char *str, ...)
{
  char filename[256];

  //  time_t time(time_t *_Nullable tloc); 
  time_t now = time(NULL);

  //  struct tm *localtime(const time_t *timep);
  struct tm *tm = localtime(&now);

  //  int snprintf(char str[restrict .size], size_t size, const char *restrict format, ...);
  snprintf(filename, sizeof(filename), "%s%04d-%02d-%02d.txt", LOG_FILE_PATH, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

  // NEW: Ensure log directory exists and is writable
  char dir_path[256];

  //  char *strncpy(char dst[restrict .sz], const char *restrict src, size_t sz);
  strncpy(dir_path, LOG_FILE_PATH, sizeof(dir_path));
  dir_path[sizeof(dir_path) - 1] = '\0';

  // Remove trailing slash if present
  /* strlen ---> calculate the length of a string */
  //  size_t strlen(const char *s);   
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

  // NEW: Set file permissions to 0644 (rw-r--r--)
  if (fchmod(fileno(file), 0644) == -1)
  {
    fprintf(stderr, "Failed to set permissions for %s: %s\n", filename, strerror(errno));
  }

  // NEW: Set ownership to zedbee:zedbee
  if (fchown(fileno(file), getuid(), getgid()) == -1)
  {
    fprintf(stderr, "Failed to set ownership for %s: %s\n", filename, strerror(errno));
  }
  fclose(file);
} 
