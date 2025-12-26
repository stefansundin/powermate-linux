# sudo apt install build-essential libpulse-dev

CC          ?= gcc
PKG_CONFIG  ?= pkg-config
CFLAGS      += -O2 -s -Wall $(shell $(PKG_CONFIG) --cflags libpulse)
LDFLAGS     += -Wl,--as-needed
LDLIBS      += $(shell $(PKG_CONFIG) --libs libpulse)
PREFIX      ?= /usr/local
BINDIR       = $(PREFIX)/bin
UDEVDIR      = /lib/udev/rules.d
INSTALL     ?= install

powermate: main.c
	$(CC) -o powermate main.c tomlc99/toml.c $(CFLAGS) $(LDLIBS)

# sudo dpkg --add-architecture i386
# sudo apt install gcc-multilib libpulse-dev:i386 libglib2.0-dev:i386
powermate32: main.c
	$(CC) -o powermate32 main.c tomlc99/toml.c $(CFLAGS) $(LDLIBS) -m32

all: powermate powermate32

release: LDFLAGS += -s
release: all

run: powermate
	./powermate -c powermate.toml

clean:
	-killall -q powermate powermate32
	rm -f powermate powermate32

install: powermate
	$(INSTALL) -D -m 755 powermate $(DESTDIR)$(BINDIR)/powermate
	$(INSTALL) -D -m 644 60-powermate.rules $(DESTDIR)$(UDEVDIR)/60-powermate.rules

.PHONY: all release run clean install
