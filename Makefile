femto: femto.c
	$(CC) femto.c -o femto -g -Wall -Wextra -Wno-strict-prototypes -pedantic -std=c99 -Iinclude/

.PHONY: clean
.PHONY: run
.PHONY: log
.PHONY: debug

clean:
	rm -f femto

run:
	./femto femto.c 2>femto.log

log:
	tail -F femto.log

debug:
	gdb -p $(pgrep femto)
