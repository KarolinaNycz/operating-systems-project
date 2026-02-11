CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g
LDFLAGS = -lm

COMMON = common.c
HEADERS = common.h

.PHONY: all clean

all: manager cashier tech fan

manager: manager.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) manager.c $(COMMON) -o manager $(LDFLAGS)

cashier: cashier.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) cashier.c $(COMMON) -o cashier $(LDFLAGS)

tech: tech.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) tech.c $(COMMON) -o tech $(LDFLAGS)

fan: fan.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) fan.c $(COMMON) -o fan $(LDFLAGS)

clean:
	rm -f manager cashier tech fan *.o raport.txt
