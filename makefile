CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I$(pwd)/vcpkg_installed/x64-linux/include
LDFLAGS = -I$(pwd)/vcpkg_installed/x64-linux/lib
PKG_CONFIG_PATH=$(pwd)/vcpkg_installed/x64-linux/lib/pkgconfig:$(pwd)/vcpkg_installed/x64-linux/share/pkgconfig:$PKG_CONFIG_PATH

SRC_DIR = src
BUILD_DIR = build
TARGET = lisp

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


clean:
	rm -rf $(BUILD_DIR) $(TARGET)

external/vcpkg/vcpkg:
	./external/vcpkg/bootstrap-vcpkg.sh

install: external/vcpkg/vcpkg
	./external/vcpkg/vcpkg install

run: $(TARGET)
	./$(TARGET)
