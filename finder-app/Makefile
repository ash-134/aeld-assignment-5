CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall

all: writer

writer: writer.c
	$(CC) $(CFLAGS)$(INCLUDES) $(OBJS) writer.c -o writer$(LDFLAGS)

clean:
	rm -f *.o writer
