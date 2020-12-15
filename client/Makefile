CC = gcc

CFLAGS_WARNINGS = \
	-pedantic -Wall -Wextra -Werror -Winline

client: main.o
	$(CC) main.o -o client $(LIBS)

client_debug: main_d.o
	$(CC) main_d.o -o client_debug $(LIBS)

main.o: main.c
	$(CC) -c $(CFLAGS_WARNINGS) -Os main.c -o main.o

main_d.o: main.c
	$(CC) -c -DDEBUG $(CFLAGS_WARNINGS) -g main.c -o main_d.o

clean:
	find . -name "*.o" -type f -delete
	find . -name "client*" -type f -delete

all: client

debug: client_debug
