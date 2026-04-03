# Kompilatory
CC       = gcc
CXX      = g++

# Flagi kompilacji
# -Wall -Wextra -Werror (zgodnie ze sztuką i wymogami)
CFLAGS   = -Wall -Wextra -Werror -O2
CXXFLAGS = -Wall -Wextra -Werror -O2 -std=c++17

# Pliki wykonywalne
CLIENT   = kayles_client
SERVER   = kayles_server

# Pliki obiektowe
# Wspólne dla obu (napisane w C)
COMMON_OBJS = common.o err.o

# Specyficzne dla programów (napisane w C++)
CLIENT_OBJS = kayles_client.o
SERVER_OBJS = kayles_server.o

# Główny cel
all: $(CLIENT) $(SERVER)

# Linkowanie klienta (używamy g++, bo on dołącza biblioteki C++)
$(CLIENT): $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Linkowanie serwera
$(SERVER): $(SERVER_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Reguła kompilacji plików .c (używa gcc)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Reguła kompilacji plików .cpp (używa g++)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Czyszczenie (zgodnie z wymogami: usuwa binarki i pliki .o)
clean:
	rm -f $(CLIENT) $(SERVER) *.o

.PHONY: all clean