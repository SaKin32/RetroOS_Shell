CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

retroshell: retroshell.c
	$(CC) $(CFLAGS) -o $@ $<

run: retroshell
	./retroshell

clean:
	rm -f retroshell

.PHONY: run clean
