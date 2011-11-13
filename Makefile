CFLAGS=-std=c99 -g  -Wall -Wno-parentheses -Werror

ALL=elm-test elm-fail

all: $(ALL)
test: $(ALL)
	for k in $^ ; do \
		valgrind --leak-check=full -q ./$$k ;\
	done

again: clean all

%-fail.o: %.c
	$(CC) $(CFLAGS) -DTEST=1 -DFAKE_FAIL=1 -c -o $@ $^

%-test.o: %.c
	$(CC) $(CFLAGS) -DTEST=1 -c -o $@ $*.c

%: %-test.o
	$(CC) $(LDFLAGS)  -o $@ $^

clean:
	rm -f $(ALL)
	rm *.o

run: elm-test elm-fail
	./n0run.py elm.c ./elm-test ;\
	./elm-fail-run.py


elm*.o: 0unit.h elm.h
