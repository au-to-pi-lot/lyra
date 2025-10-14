CC = gcc

ifdef RELEASE
CFLAGS = -Wall -Wextra -std=c11 -O3 -DNDEBUG -flto -Ivcpkg_installed/x64-linux/include
LDFLAGS = -Lvcpkg_installed/x64-linux/lib -flto
STRIP = strip
else
CFLAGS = -Wall -Wextra -std=c11 -g -Ivcpkg_installed/x64-linux/include
LDFLAGS = -Lvcpkg_installed/x64-linux/lib
STRIP = @true
endif

SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests
TARGET = lyra

SRCS = $(shell find $(SRC_DIR) -name '*.c')
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Test configuration
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)
LIB_OBJS = $(filter-out $(BUILD_DIR)/main.o, $(OBJS))

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) -ledit -lncurses
	$(STRIP) $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


# Test targets
test: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done

$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS) -lcheck -lm

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

external/vcpkg/vcpkg:
	./external/vcpkg/bootstrap-vcpkg.sh

install: external/vcpkg/vcpkg
	./external/vcpkg/vcpkg install

run: $(TARGET)
	./$(TARGET)
