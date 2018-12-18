# sudo apt-get install libpulse-dev

CFLAGS = -O2 -s -Wall -Wl,--as-needed $(shell pkg-config --cflags libpulse)
LIBS = $(shell pkg-config --libs libpulse)

powermate: main.c
	gcc -o powermate main.c tomlc99/toml.c $(CFLAGS) $(LIBS)

# sudo apt-get install libc6-dev-i386 libpulse-dev:i386 libglib2.0-dev:i386
powermate32: main.c
	gcc -o powermate32 main.c tomlc99/toml.c $(CFLAGS) $(LIBS) -m32

all: powermate powermate32

clean:
	-killall -q powermate powermate32
	rm -f powermate powermate32
