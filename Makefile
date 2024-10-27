femto: femto.c
	$(CC) femto.c -o femto -Wall -Wextra -Wno-strict-prototypes -pedantic -std=c99 -Iinclude/

.PHONY: clean

.PHONY: run

clean:
	rm -f femto

run:
	./femto femto.c 2>femto.log
