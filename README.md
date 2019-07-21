# PowerMate Linux Driver

This is a small Linux userspace driver for the [Griffin PowerMate](https://griffintechnology.com/powermate-usb) (USB version only). It requires PulseAudio to work (default in Ubuntu).

For an easy to use PPA, use: https://launchpad.net/~stefansundin/+archive/ubuntu/powermate


# Disclaimer

PowerMate is a registered trademark of Griffin Technologies Inc. This software is not affiliated or endorsed by them.

This software is written by Stefan Sundin and is licensed under GPLv3.


# Installation

Binaries are available [in the releases section](https://github.com/stefansundin/powermate-linux/releases).

## Clone Repository

```bash
$ git clone --recursive-submodules https://github.com/stefansundin/powermate-linux.git
$ cd powermate-linux
```

## Setup udev rule

```bash
$ sudo cp 60-powermate.rules /etc/udev/rules.d/
```

After copying the file, unplug and plug the device back in.

## Compile

Install requirements and then compile:

```bash
$ sudo apt-get install libpulse-dev libnotify-dev
$ make
```

## Run

Simply run the executable with `./powermate`.

The program should be able to handle the device being disconnected and connected without problems.

Make it run automatically on login somehow. If you are running a standard desktop environment then there is usually a "Startup Applications" preference. This is automatically installed if you use the PPA.

You can use `./powermate -d` to daemonize the program without having to update the config file.


# Configuration

By default, the program changes the volume when you turn the knob clockwise and counter clockwise, but you are able to customize the program to do anything via [the configuration file](powermate.toml).

Press the knob to mute. Hold down the knob for one second to activate "movie mode". Operation is the same except the LED stays off.
