CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
LDFLAGS = -lrt -lpthread

SRC_DIR = src
BIN_DIR = bin

DAEMON_SRC = $(SRC_DIR)/daemon.c
WORKER_SRC = $(SRC_DIR)/worker.c
COMMON_HDR = $(SRC_DIR)/common.h

DAEMON_BIN = $(BIN_DIR)/daemon
WORKER_BIN = $(BIN_DIR)/worker

all: build $(DAEMON_BIN) $(WORKER_BIN)

build:
	mkdir -p $(BIN_DIR)

$(DAEMON_BIN): $(DAEMON_SRC) $(COMMON_HDR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(WORKER_BIN): $(WORKER_SRC) $(COMMON_HDR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

re: clean all

.PHONY: all clean re build