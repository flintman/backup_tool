# Makefile for backup_manager (C++17)

CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -std=c++17
LDFLAGS ?=

SRC_DIR := src
INC_DIR := includes
BIN := backup_manager
BIN2 := backup_cleanup

BM_SRCS := $(SRC_DIR)/backup_manager.cpp $(SRC_DIR)/main.cpp
BM_OBJS := $(BM_SRCS:.cpp=.o)

BC_SRCS := $(SRC_DIR)/backup_cleanup.cpp $(SRC_DIR)/cleanup_main.cpp
BC_OBJS := $(BC_SRCS:.cpp=.o)

.PHONY: all clean run install

all: $(BIN) $(BIN2)


$(BIN): $(BM_OBJS)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c -o $@ $<

run: $(BIN)
	./$(BIN)

install: $(BIN)
	install -m 0755 $(BIN) /usr/local/bin/$(BIN)

clean:
	rm -f $(BM_OBJS) $(BC_OBJS) $(BIN) $(BIN2)

# backup_cleanup standalone
$(BIN2): $(BC_OBJS)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -o $@ $^ $(LDFLAGS)
