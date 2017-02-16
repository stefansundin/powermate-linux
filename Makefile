# sudo apt-get install libpulse-dev libnotify-dev

CFLAGS = -O2 -s -Wall $(shell pkg-config --cflags libpulse libnotify)
LIBS = `pkg-config --libs libpulse libnotify` -lxdo

powermate: main.c
	gcc -o powermate main.c $(CFLAGS) $(LIBS)

# sudo apt-get install libc6-dev-i386 libpulse-dev:i386 libglib2.0-dev:i386 libavahi-client-dev:i386 libnotify-dev:i386
powermate32: main.c
	gcc -o powermate32 main.c $(CFLAGS) $(LIBS) -m32

all: powermate powermate32

clean:
	-killall -q powermate powermate32
	rm -f powermate powermate32
