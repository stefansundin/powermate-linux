# PowerMate Linux Driver

This is a small Linux userspace driver for the [Griffin PowerMate](https://store.griffintechnology.com/desktop/powermate). Tested only on Ubuntu 14.10.


# Disclaimer

PowerMate is a registered trademark of Griffin Technologies Inc. This software is not affiliated or endorsed by them.

This software is written by Stefan Sundin and is licensed under GPLv3.


# Installation

Create a udev rule by creating the file `/etc/udev/rules.d/60-powermate.rules`

```
ACTION=="add", ENV{ID_USB_DRIVER}=="powermate", SYMLINK+="input/powermate", MODE="0666"
```

After creating the file, unplug and plug the device back in.

Download the binary [here](https://github.com/stefansundin/powermate-linux/releases/latest).


# Run

Simply run the executable with `./powermate`.

Make it run automatically on login somehow. The program can handle the device being disconnected and connected without problems.


# Changes

[![RSS](https://stefansundin.github.io/img/feed.png) Release feed](https://github.com/stefansundin/powermate-linux/releases.atom)

**v2** - 2015-04-11 - [diff](https://github.com/stefansundin/powermate-linux/compare/v1...v2):
- The LED is now updated when the volume changes.
- The program now uses PulseAudio directly. If this is an issue for you, use [v1](https://github.com/stefansundin/powermate-linux/releases/tag/v1).

**v1** - 2015-04-09:
- Initial release.


# TODO

- [ ] Run program from udev rule (causes pulseaudio connect error now).
- [ ] Create deb and ppa.
