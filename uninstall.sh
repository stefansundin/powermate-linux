#!/bin/bash

killall powermate

sudo rm -v $HOME/.local/share/applications/powermate.desktop
sudo rm -v /usr/bin/powermate
sudo rm -v ~/.config/powermate.toml
sudo rm -rv /opt/powermate
sudo rm -v /etc/udev/rules.d/60-powermate.rules

echo
echo "Uninstall complete"
