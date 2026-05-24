CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17 -Iinclude -fPIC
LDLIBS = -lssl -lcrypto -lboost_url

TARGET = sikradio

SRC = \
    sikradio.cpp \
    args.cpp \
    client.cpp \
    connection.cpp \
    http.cpp \
    logger.cpp \
    io.cpp

BUILD_DIR = .
OBJ_DIR = obj

OBJ = $(SRC:%.cpp=$(OBJ_DIR)/%.o)

# ======================
# DEFAULT
# ======================
all: $(TARGET) $(LIB)

# ======================
# EXECUTABLE
# ======================
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)



# ======================
# COMPILE OBJECT FILES
# ======================
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ======================
# CLEAN
# ======================
clean:
	rm -rf obj $(TARGET) $(LIB)

# ======================
# DEBUG BUILD
# ======================
debug: CXXFLAGS += -g -O0
debug: clean all

# ======================
# ASAN BUILD
# ======================
asan: CXXFLAGS += -g -O1 -fsanitize=address
asan: LDLIBS +=
asan: clean all

# ======================
# RUN WITH VALGRIND
# ======================
valgrind: $(TARGET)
	valgrind --leak-check=full --track-origins=yes ./$(TARGET)

.PHONY: all clean debug asan valgrind