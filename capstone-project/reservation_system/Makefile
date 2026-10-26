CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS = -lws2_32
COMMON = reservation.o seatmap.o storage.o

all: server.exe client.exe

server.exe: server.o $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

client.exe: client.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o server.exe client.exe

.PHONY: all clean
