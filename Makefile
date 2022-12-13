CC = gcc

CFLAGS = -Wall -Wextra -g 

.PHONY: all clean

all: configurator slave master

configurator: configurator.o

configurator.o: configurator.c utils.h utils_configurator.h

master: master.o

master.o: master.c utils.h

slave: slave.o 

slave.o: slave.c utils.h

clean:
	-rm -f *.o *~ configurator slave master