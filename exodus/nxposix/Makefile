CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2
SANITIZERS ?= -fsanitize=undefined
BUILD_DIR ?= build
TEST_BIN := $(BUILD_DIR)/test_reactor
TCP_BIN := $(BUILD_DIR)/tcp_smoke

.PHONY: all test clean
all: test

$(TEST_BIN): reactor.c reactor.h test_reactor.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZERS) -I. reactor.c test_reactor.c -o $@

$(TCP_BIN): reactor.c reactor.h tcp_smoke.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZERS) -I. reactor.c tcp_smoke.c -o $@

test: $(TEST_BIN) $(TCP_BIN)
	./$(TEST_BIN)
	./$(TCP_BIN)

clean:
	rm -rf $(BUILD_DIR)
