# ELM0 website examples
#
# See README for an explanation of what ELM0 is.
#
#    make               builds the examples
#    make test 	        builds elm and runs full n0run unit tests.
#    make clean         deletes all built files
#
# Copyright (C) 2012, Adrian Ratnapala, under the ISC license. See file LICENSE.
#

TEST_PROGS=errors logging malloc 0unit
TEST_OBJS = $(TEST_PROGS:%=$(BUILD_DIR)/%)

CFLAGS = -std=c99 $(OPTFLAGS) -Wall -Ielm0/include -Wno-parentheses
LDFLAGS = -Lelm0/lib
LDLIBS = -lelm

all: elm examples
examples: $(TEST_PROGS)

test: all
	for p in $(TEST_PROGS); do ./$$p; done

elm:
	INSTALL_DIR=$(PWD)/elm0 BUILD_DIR=$(PWD)/build make -C src clean install

clean:
	rm -f $(TEST_PROGS)
	rm -rf build
	rm -rf elm0

