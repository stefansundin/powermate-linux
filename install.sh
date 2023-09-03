#!/bin/bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"

sudo apt install xdotool
sudo cp 60-powermate.rules /etc/udev/rules.d/
sudo apt-get install libpulse-dev libnotify-dev
make
sudo cp -r ./ /opt/powermate
wget https://upload.wikimedia.org/wikipedia/commons/thumb/7/7d/Input-wheel-griffin-powermate.svg/480px-Input-wheel-griffin-powermate.svg.png -O $DIR/powermate.png
sudo cp -r $DIR/powermate.png /opt/powermate/

sudo ln -s /opt/powermate/powermate.toml ~/.config/powermate.toml

sudo ln -s /opt/powermate/powermate /usr/bin/powermate

echo "#!/usr/bin/env xdg-open
[Desktop Entry]
Version=1.0
Terminal=false
Type=Application
Name=Powermate
Description=A small Linux userspace driver for the USB Griffin PowerMate. See: https://github.com/stefansundin/powermate-linux
Exec=powermate -d
Icon=/opt/powermate/powermate.png" >$HOME/.local/share/applications/powermate.desktop
echo
killall powermate
powermate
echo
echo "You can now delete the downloaded directory"
