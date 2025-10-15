.PHONY: default clean

default:
	cc -std=c11 -O2 -Wall -Wextra `pkg-config --cflags --libs sdl3` -o xorx xorx.c

clean:
	rm -f xorx
