# Makefile for backup_manager (C++17)

CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -std=c++17
LDFLAGS ?=
LIBS = -lcurl

SRC_DIR := src
INC_DIR := includes
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
BIN := $(BIN_DIR)/bmanager

BM_SRCS := $(SRC_DIR)/backup_manager.cpp
BM_OBJS := $(BM_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(BIN)

$(BIN): $(BM_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -o $@ $^ $(LDFLAGS) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)