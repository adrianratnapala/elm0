CFLAGS=-std=c99 -g  -Wall -Wno-parentheses -Werror -Wno-implicit-function-declaration

ALL=elm-test elm-fail

all: $(ALL)

# testing without n0run.
simple-test: $(ALL)
	for k in $^ ; do \
		valgrind --leak-check=full -q ./$$k ;\
	done

again: clean all

%-fail.o: %.c
	$(CC) $(CFLAGS) -DTEST=1 -DFAKE_FAIL=1 -c -o $@ $^
#	$(CC) $(CFLAGS) -DFAKE_FAIL=1 -c -o $@ $^

%-test.o: %.c
	$(CC) $(CFLAGS) -DTEST=1 -c -o $@ $*.c
#
#%: %-test.o
#	$(CC) $(LDFLAGS)  -o $@ $^

%-test: %-test.o test_%.o
	$(CC) $(LDFLAGS)  -o $@ $^

%-fail: %-fail.o test_%-fail.o
	$(CC) $(LDFLAGS)  -o $@ $^

clean:
	rm -f $(ALL)
	rm -f *.o

run: elm-test elm-fail
	./n0run.py test_elm.c ./elm-test ;\
	./elm-fail-run.py


elm*.o: 0unit.h elm.h
