BUILD_DIR ?= .
INSTALL_DIR ?= $(BUILD_DIR)

LIBS=elm.a
TEST_PROGS=elm-test elm-fail

OPTFLAGS ?= -g -Werror
CFLAGS = -std=c99 $(OPTFLAGS) -Wall -Wno-parentheses


TEST_TARGETS = $(TEST_PROGS:%=$(BUILD_DIR)/%)
LIB_TARGETS = $(LIBS:%=$(BUILD_DIR)/%)


all: dirs $(LIB_TARGETS)
test_progs: dirs $(TEST_TARGETS)

$(BUILD_DIR)/%-fail.o: %.c
	$(CC) $(CFLAGS) -DFAKE_FAIL=1 -c -o $@ $^

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

%-test: %.o test_%.o
	$(CC) $(LDFLAGS)  -o $@ $^

%-fail: %-fail.o test_%-fail.o
	$(CC) $(LDFLAGS)  -o $@ $^

clean:
	rm -f $(TEST_TARGETS) $(LIB_TARGETS)
	rm -f *.o

run: test_progs
	TEST_DIR=$(BUILD_DIR) ./n0run.py test_elm.c ./elm-test ;\
	TEST_DIR=$(BUILD_DIR) ./elm-fail-run.py

%.a: %.o
	ar rcs $@ $^

$(BUILD_DIR)/*.o: 0unit.h elm.h

dirs:
	mkdir -p $(BUILD_DIR)

install: all
	mkdir -p $(INSTALL_DIR)/lib
	mkdir -p $(INSTALL_DIR)/include
	install -m 664 -t $(INSTALL_DIR)/lib     $(BUILD_DIR)/elm.a
	install -m 664 -t $(INSTALL_DIR)/include elm.h 0unit.h

