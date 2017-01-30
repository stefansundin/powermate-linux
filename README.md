# PowerMate Linux Driver

This is a small Linux userspace driver for the [Griffin PowerMate](https://griffintechnology.com/us/products/audio/powermate). Tested only on Ubuntu 14.10, 15.04, and 15.10.

For an easy to use PPA, use: https://launchpad.net/~stefansundin/+archive/ubuntu/powermate


# Disclaimer

PowerMate is a registered trademark of Griffin Technologies Inc. This software is not affiliated or endorsed by them.

This software is written by Stefan Sundin and is licensed under GPLv3.


# Installation

## Clone Git Repository to Working Directory
```bash
$ git clone https://github.com/stefansundin/powermate-linux.git
```
This will clone the latest version of the PowerMate drivers to your current directory.

cd into the new directory
```bash
$ cd powermate-linux
```


## Set udev rule
```bash
$ sudo cp 60-powermate.rules /etc/udev/rules.d 
```

After copying the file, unplug and plug the device back in. 

## Compile
Install library requirements and then compile:
```bash 
$ sudo apt-get install libpulse-dev libnotify-dev
$ make
```

## Run

Simply run the executable with `./powermate`

Make it run automatically on login somehow. The program can handle the device being disconnected and connected without problems. 
One method to automatically run the program on boot is using cron. To set this up, run the following commands:
```bash
$ crontab -e 
```

Then, add this line to your crontab substituting in 'PATH_TO_INSTALL' with your specific directory containing the powermate program:

```
@reboot /home/$USER/PATH_TO_INSTALL/powermate
```



# Changes

[![RSS](https://stefansundin.github.io/img/feed.png) Release feed](https://github.com/stefansundin/powermate-linux/releases.atom)

**v5** - 2015-12-16 - [diff](https://github.com/stefansundin/powermate-linux/compare/v4...v5):
- Properly detect new sink volume.

**v4** - 2015-12-16 - [diff](https://github.com/stefansundin/powermate-linux/compare/v3...v4):
- Detect sink change.
- Handle louder than 100% better.

**v3** - 2015-04-24 - [diff](https://github.com/stefansundin/powermate-linux/compare/v2...v3):
- Movie mode -- hold knob down for one second to turn off LED.
- Device no longer has to be connected when program starts.
- Program is now single-threaded.

**v2** - 2015-04-11 - [diff](https://github.com/stefansundin/powermate-linux/compare/v1...v2):
- The LED is now updated when the volume changes.
- The program now uses PulseAudio directly. If this is an issue for you, use [v1](https://github.com/stefansundin/powermate-linux/releases/tag/v1).

**v1** - 2015-04-09:
- Initial release.
