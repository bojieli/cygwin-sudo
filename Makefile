CFLAGS=-Wall -g -O2

default:
	gcc ${CFLAGS} -o sudo sudo.c
	gcc ${CFLAGS} -o sudo-server sudo-server.c

uac:
	g++ ${CFLAGS} -o uac uac.cpp

clean:
	@rm sudo.exe sudo-server.exe uac.exe
