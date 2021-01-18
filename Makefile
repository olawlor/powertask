OPTS=-g
CFLAGS=-Wall $(OPTS)
CC=gcc

all: run

powertask_example: *.c *.h
	$(CC) $(CFLAGS) powertask_builtin.c example_ABC.c -o $@

run: powertask_example
	./powertask_example

clean:
	- rm powertask_example
