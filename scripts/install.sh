#!/bin/sh

echo "Installing distributed music player system..."
echo "Make sure that your BBG has an internet connection and that this is run with 'sudo' or root permissions"

# verify that python is installed
which python >/dev/null
if [ $? -eq 1 ]; then
        echo "Did not install, python is needed"
        return 1
fi

# verify that curl is installed
which curl >/dev/null
if [ $? -eq 1 ]; then
        echo "Did not install, curl is needed"
        return 1
fi

# install youtube-dl
which youtube-dl >/dev/null
if [ $? -eq 1 ]; then
        curl -L https://yt-dl.org/downloads/latest/youtube-dl -o /usr/local/bin/youtube-dl
        chmod a+rx /usr/local/bin/youtube-dl
        apt-get install libav-tools -y
fi

# install music player
mkdir -p /root/cache
mkdir -p /root/musicPlayer
cp /mnt/remote/myApps/musicPlayer /root/musicPlayer/
cp -r /mnt/remote/myApps/music-player-nodejs-copy /root/musicPlayer/nodejs
cp /mnt/remote/myApps/musicPlayerServices/musicApp.service /etc/systemd/system/
cp /mnt/remote/myApps/musicPlayerServices/musicWeb.service /etc/systemd/system/

# set services to start during startup
systemctl enable musicApp.service
systemctl enable musicWeb.service

echo "Installation completed successfully!"
echo "It is recommended to reboot your device now"
