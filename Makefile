CC = gcc

CFLAGS_WARNINGS = \
	-pedantic -Wall -Wextra -Werror -Winline

LIBS =

ftp_server: main.c
	$(CC) main.c -o ftp_server -Os $(CFLAGS_WARNINGS) $(LIBS)

ftp_server_debug: main.c
	$(CC) main.c -o ftp_server_debug -DDEBUG $(CFLAGS_WARNINGS) -g $(LIBS)

clean:
	rm ftp_server
	rm ftp_server_debug

all: ftp_server

debug: ftp_server_debug
