CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17 -Iinclude -fPIC
# Add the external libraries here
LDLIBS = -lssl -lcrypto -lboost_url

TARGET = sikradio
LIB = libargs.so

SRC = \
    sikradio.cpp \
    src/args.cpp \
    src/client.cpp \
    src/connection.cpp \
    src/protocol.cpp \
    src/logger.cpp \
    src/io.cpp

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

OBJ = $(SRC:%.cpp=$(OBJ_DIR)/%.o)

# ======================
# DEFAULT
# ======================
all: $(TARGET) $(LIB)

# ======================
# EXECUTABLE
# ======================
$(TARGET): $(OBJ)
	# Added $(LDLIBS) to the very end of this line
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# ======================
# SHARED LIB (ONLY args.cpp here, adjust if needed)
# ======================
$(LIB): $(OBJ_DIR)/src/args.o
	# If your shared library also relies on boost_url, you need it here too!
	$(CXX) -shared -o $@ $^ -lboost_url

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
	rm -rf $(BUILD_DIR) $(TARGET) $(LIB)

.PHONY: all clean