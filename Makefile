# Makefile for backup_manager (C++17)

# SPDX-License-Identifier: MIT
# Copyright (c) 2025 William C. Bellavance Jr.
CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -std=c++17
LDFLAGS ?=
LIBS = -lcurl

SRC_DIR := src
INC_DIR := includes
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TARGET := bmanager
VERSION := 1.0.3
BIN := $(BIN_DIR)/$(TARGET)
ARCH=$(shell dpkg-architecture -qDEB_BUILD_ARCH)
DEBDIR=$(BUILD_DIR)/debian/$(TARGET)


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

deb: all
	mkdir -p $(DEBDIR)/usr/bin
	mkdir -p $(DEBDIR)/DEBIAN
	cp $(BIN) $(DEBDIR)/usr/bin/
	mkdir -p $(DEBDIR)/usr/share/backup_manager
	cp backup.env $(DEBDIR)/usr/share/backup_manager/backup.env
	cp LICENSE $(DEBDIR)/usr/share/backup_manager/LICENSE
	cp VERSION.md $(DEBDIR)/usr/share/backup_manager/VERSION.md

	echo "Package: $(TARGET)" > $(DEBDIR)/DEBIAN/control
	echo "Version: $(VERSION)" >> $(DEBDIR)/DEBIAN/control
	echo "Section: base" >> $(DEBDIR)/DEBIAN/control
	echo "Priority: optional" >> $(DEBDIR)/DEBIAN/control
	echo "Architecture: $(ARCH)" >> $(DEBDIR)/DEBIAN/control
	echo "Maintainer: William Bellavance <flintman@flintmancomputers.com>" >> $(DEBDIR)/DEBIAN/control
	echo "Description: Backup Service" >> $(DEBDIR)/DEBIAN/control
	echo "Depends: tar, sshpass, rsync, curl" >> $(DEBDIR)/DEBIAN/control

	echo "#!/bin/bash" > $(DEBDIR)/DEBIAN/postinst

	echo "if [ ! -f \"$(HOME)/backup/backup.env\" ]; then" >> $(DEBDIR)/DEBIAN/postinst
	echo "  mkdir -p \"$(HOME)/backup\"" >> $(DEBDIR)/DEBIAN/postinst
	echo "  cp /usr/share/backup_manager/backup.env \"$(HOME)/backup/backup.env\"" >> $(DEBDIR)/DEBIAN/postinst
	echo "  echo \"NOTE: The default backup.env has been copied to your backup directory.\"" >> $(DEBDIR)/DEBIAN/postinst
	echo "  echo \"Please update $(HOME)/backup/backup.env with your own settings.\"" >> $(DEBDIR)/DEBIAN/postinst
	echo "fi" >> $(DEBDIR)/DEBIAN/postinst
	chmod 755 $(DEBDIR)/DEBIAN/postinst

	dpkg-deb --build $(DEBDIR) $(BUILD_DIR)/$(TARGET)_$(VERSION)_$(ARCH).deb

clean:
	rm -rf $(BUILD_DIR)