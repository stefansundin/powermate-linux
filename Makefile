# sudo apt install build-essential libpulse-dev

CFLAGS = -O2 -s -Wall -Wl,--as-needed $(shell pkg-config --cflags libpulse)
LIBS = $(shell pkg-config --libs libpulse)

powermate: main.c
	gcc -o powermate main.c tomlc99/toml.c $(CFLAGS) $(LIBS)

# sudo dpkg --add-architecture i386
# sudo apt install gcc-multilib libpulse-dev:i386 libglib2.0-dev:i386
powermate32: main.c
	gcc -o powermate32 main.c tomlc99/toml.c $(CFLAGS) $(LIBS) -m32

all: powermate powermate32

run: powermate
	./powermate -c powermate.toml

clean:
	-killall -q powermate powermate32
	rm -f powermate powermate32

.PHONY: all run clean
