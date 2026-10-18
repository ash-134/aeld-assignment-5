CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt

all: $(TARGET)
default: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(TARGET)
