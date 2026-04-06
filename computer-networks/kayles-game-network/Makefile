# -------------------------------
# Makefile for Kayles client & server
# -------------------------------

# Compilers
CC     = gcc
CXX    = g++

# Flags
CFLAGS   = -Wall -Wextra -O2 -std=gnu17 -g -fsanitize=address
CXXFLAGS = -Wall -Wextra -O2 -std=c++17 -g -fsanitize=address

# Include directories
INCDIRS = -I/usr/local/include

# Libraries for server
LIBS    = -lpthread        # only link libraries you actually have
LDFLAGS_SERVER = -fsanitize=address $(LIBS)

# Targets
TARGET_CLIENT = kayles_client
TARGET_SERVER = kayles_server

# Source files
C_SRCS   = err.c common.c protocol.c game.c kayles_client.c
CPP_SRCS = kayles_server.cpp

OBJ_C   = $(C_SRCS:.c=.o)
OBJ_CPP = $(CPP_SRCS:.cpp=.o)

.PHONY: all clean

# Default target
all: $(TARGET_CLIENT) $(TARGET_SERVER)

# Link client
$(TARGET_CLIENT): $(OBJ_C)
	$(CC) $(CFLAGS) -o $@ $^

# Link server
# Note: server object depends on all C objects except kayles_client.o
$(TARGET_SERVER): $(OBJ_CPP) $(filter-out kayles_client.o, $(OBJ_C))
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS_SERVER)

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) $(INCDIRS) -c $< -o $@

# Compile C++ files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCDIRS) -c $< -o $@

# Clean build files
clean:
	rm -f $(TARGET_CLIENT) $(TARGET_SERVER) *.o *~