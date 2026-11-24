CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2
SANITIZERS ?= -fsanitize=undefined
BUILD_DIR ?= build
TEST_BIN := $(BUILD_DIR)/test_reactor
TCP_BIN := $(BUILD_DIR)/tcp_smoke
HANDLE_BIN := $(BUILD_DIR)/test_handle_wait

.PHONY: all test clean
all: test

$(TEST_BIN): reactor.c reactor.h test_reactor.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZERS) -I. reactor.c test_reactor.c -o $@

$(TCP_BIN): reactor.c reactor.h tcp_smoke.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZERS) -I. reactor.c tcp_smoke.c -o $@

$(HANDLE_BIN): handle_wait.c handle_wait.h test_handle_wait.c mock/switch.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZERS) -Imock -I. handle_wait.c test_handle_wait.c -o $@

test: $(TEST_BIN) $(TCP_BIN) $(HANDLE_BIN)
	./$(TEST_BIN)
	./$(TCP_BIN)
	./$(HANDLE_BIN)

clean:
	rm -rf $(BUILD_DIR)
