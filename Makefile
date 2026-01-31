CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

COMMON = common.c
HEADERS = common.h

.PHONY: all clean

all: manager cashier tech fan

manager: manager.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) manager.c $(COMMON) -o manager

cashier: cashier.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) cashier.c $(COMMON) -o cashier

tech: tech.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) tech.c $(COMMON) -o tech

fan: fan.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) fan.c $(COMMON) -o fan

clean:
	rm -f manager cashier tech fan
