# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -Wall -g -I.

# Linker flags
LDFLAGS = -lmodbus -lpaho-mqtt3c -lcjson -pthread -lm

# Target executable name
TARGET = renu_sen

# Source files
SOURCES = main.c config.c server.c co2_client.c mqtt.c jsonify.c boodskap.c

# Object files (generated from source files)
OBJECTS = $(SOURCES:.c=.o)

# Header files
HEADERS = config.h server.h co2_client.h data.h mqtt.h jsonify.h boodskap.h

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files to object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and executable
clean:
	rm -f $(OBJECTS) $(TARGET)

# Phony targets (not associated with files)
.PHONY: all clean
