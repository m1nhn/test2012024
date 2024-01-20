# Makefile for UDP Authentication Server and Client

CC = gcc
LDFLAGS =
TARGETS = server client

all: $(TARGETS)

server: server.c
	$(CC) -o server server.c

client: client.c
	$(CC) -o client client.c

clean:
	rm -f $(TARGETS)
