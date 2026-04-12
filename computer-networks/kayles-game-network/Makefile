# Compiler and flags
CXX = g++
# Set to c++23 for the latest features, including full span support
CXXFLAGS = -Wall -Wextra -O2 -std=c++23
LDFLAGS =

# Module objects
MODULE_OBJS = common.o err.o game.o protocol.o

# Targets
TARGETS = kayles_client kayles_server

.PHONY: all clean

all: $(TARGETS)

# Link the client
kayles_client: kayles_client.o $(MODULE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Link the server
kayles_server: kayles_server.o $(MODULE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Rule for object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)