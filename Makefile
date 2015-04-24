# sudo apt-get install libpulse-dev

CFLAGS = -lpulse -O2 -s -Wall

powermate: main.c
	gcc -o powermate main.c $(CFLAGS)

# sudo apt-get install libc6-dev-i386 libpulse-dev:i386 libglib2.0-dev:i386
powermate32: main.c
	gcc -o powermate32 main.c $(CFLAGS) -m32

all: powermate powermate32

clean:
	-killall -q powermate powermate32
	rm -f powermate powermate32
