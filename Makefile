MAKE_DIR = $(PWD)
SRC_DIR = $(MAKE_DIR)/src
LIB_DIR = $(MAKE_DIR)/libs
BUILD_DIR = $(MAKE_DIR)/build

INC_SRCH_PATH = -I$(SRC_DIR)

LIB_SRCH_PATH :=
LIB_SRCH_PATH += -L$(LIB_DIR)

CC = gcc

LIBS := -lhttp -lthpool -lcache -lpthread

CFLAGS :=
CFLAGS += $(INC_SRCH_PATH) $(LIB_SRCH_PATH)
CFLAGS += -Wall

.PHONY: all clean dir debug test

debug: CFLAGS += -g

export MAKE_DIR SRC_DIR LIB_DIR BUILD_DIR CC CFLAGS LIBS INC_SRCH_PATH

all: dir
	@$(MAKE) -C src/http -f http.mk
	@$(MAKE) -C src/thpool -f thpool.mk
	@$(MAKE) -C src/cache -f cache.mk
	@$(MAKE) -C src/root -f root.mk

debug: all

test:
	@test/all_test.sh

dir:
	@mkdir -p $(LIB_DIR)
	@mkdir -p $(BUILD_DIR)

clean:
	@$(MAKE) -C src/http -f http.mk clean
	@$(MAKE) -C src/thpool -f thpool.mk clean
	@$(MAKE) -C src/cache -f cache.mk clean
	@$(MAKE) -C src/root -f root.mk clean
