.PHONY: all build test clean install

CC ?= cc
PREFIX ?= /usr/local
BUILD_DIR ?= build
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -g
CPPFLAGS ?= -I.
LDFLAGS ?= -lz

CORE_SRC = \
	commits/avc_commit.c \
	compression/avc_compress.c \
	config/avc_config.c \
	hashing/avc_hash.c \
	index/avc_index.c \
	objects/avc_oid.c \
	objects/avc_object.c \
	refs/avc_refs.c \
	repository/avc_repository.c \
	utils/avc_error.c \
	utils/avc_fs.c \
	utils/avc_log.c

CLI_SRC = cli/avc_cli.c cli/main.c
TEST1_SRC = tests/unit/test_phase1.c
TEST2_SRC = tests/unit/test_phase2.c
TEST3_SRC = tests/unit/test_phase3.c
TEST4_SRC = tests/unit/test_phase4.c

CORE_OBJ = $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)
CLI_OBJ = $(CLI_SRC:%.c=$(BUILD_DIR)/%.o)
TEST1_OBJ = $(TEST1_SRC:%.c=$(BUILD_DIR)/%.o)
TEST2_OBJ = $(TEST2_SRC:%.c=$(BUILD_DIR)/%.o)
TEST3_OBJ = $(TEST3_SRC:%.c=$(BUILD_DIR)/%.o)
TEST4_OBJ = $(TEST4_SRC:%.c=$(BUILD_DIR)/%.o)

all: build

build: $(BUILD_DIR)/astraphosvc

$(BUILD_DIR)/astraphosvc: $(CORE_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_phase1: $(CORE_OBJ) $(TEST1_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_phase2: $(CORE_OBJ) $(TEST2_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_phase3: $(CORE_OBJ) $(TEST3_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_phase4: $(CORE_OBJ) $(TEST4_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

test: $(BUILD_DIR)/test_phase1 $(BUILD_DIR)/test_phase2 $(BUILD_DIR)/test_phase3 $(BUILD_DIR)/test_phase4 $(BUILD_DIR)/astraphosvc
	$(BUILD_DIR)/test_phase1
	$(BUILD_DIR)/test_phase2
	$(BUILD_DIR)/test_phase3
	$(BUILD_DIR)/test_phase4
	sh tests/integration/test_phase1_cli.sh $(BUILD_DIR)/astraphosvc

install: build
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(BUILD_DIR)/astraphosvc $(DESTDIR)$(PREFIX)/bin/astraphosvc

clean:
	rm -rf $(BUILD_DIR)
