# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra

# Libraries
LIBS = -lmodbus -lgpiod

# Sources
SRCS = main.c gpio_control.c
OBJS = $(SRCS:.c=.o)

# Output binary
TARGET = hat_modbus

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) # $(TARGET)

.PHONY: all clean

