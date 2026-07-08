.PHONY: all build test clean install

CC ?= cc
PREFIX ?= /usr/local
BUILD_DIR ?= build
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -g
CPPFLAGS ?= -I.

CORE_SRC = \
	config/avc_config.c \
	repository/avc_repository.c \
	utils/avc_error.c \
	utils/avc_fs.c \
	utils/avc_log.c

CLI_SRC = cli/avc_cli.c cli/main.c
TEST_SRC = tests/unit/test_phase1.c

CORE_OBJ = $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)
CLI_OBJ = $(CLI_SRC:%.c=$(BUILD_DIR)/%.o)
TEST_OBJ = $(TEST_SRC:%.c=$(BUILD_DIR)/%.o)

all: build

build: $(BUILD_DIR)/astraphosvc

$(BUILD_DIR)/astraphosvc: $(CORE_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/test_phase1: $(CORE_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

test: $(BUILD_DIR)/test_phase1 $(BUILD_DIR)/astraphosvc
	$(BUILD_DIR)/test_phase1
	sh tests/integration/test_phase1_cli.sh $(BUILD_DIR)/astraphosvc

install: build
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(BUILD_DIR)/astraphosvc $(DESTDIR)$(PREFIX)/bin/astraphosvc

clean:
	rm -rf $(BUILD_DIR)
