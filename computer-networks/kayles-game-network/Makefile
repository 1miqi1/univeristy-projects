# -------------------------------
# Makefile for Kayles client & server
# -------------------------------

CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu17  -g -fsanitize=address
LDFLAGS = -fsanitize=address

.PHONY: all clean

TARGET1 = kayles_client
TARGET2 = kayles_server

all: $(TARGET1) $(TARGET2)

# Linking targets
$(TARGET1): kayles_client.o common.o err.o protocol.o game.o
	$(CC) $(CFLAGS) -g -o $@ $^

$(TARGET2): kayles_server.o common.o err.o protocol.o game.o
	$(CC) $(CFLAGS) -g -o $@ $^

# Explicit dependencies

# err
err.o: err.c err.h

# common
common.o: common.c err.h common.h

# protocol
protocol.o: protocol.c protocol.h err.h common.h

# game
game.o: game.c game.h err.h common.h

# client
kayles_client.o: kayles_client.c protocol.h err.h common.h game.h

# server
kayles_server.o: kayles_server.c protocol.h err.h common.h game.h

# Clean build files
clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~