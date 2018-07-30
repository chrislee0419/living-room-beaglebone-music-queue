#!/bin/sh

echo "Installing distributed music player system..."
echo "Make sure that your BBG has an internet connection and that this is run with 'sudo' or root permissions"

# verify that python is installed
which python >/dev/null
if [ $? -eq 1 ]; then
        echo "Did not install, python is needed"
        return 1
fi

# install youtube-dl
which youtube-dl >/dev/null
if [ $? -eq 1 ]; then
        # verify that curl is installed
        which curl >/dev/null
        if [ $? -eq 1 ]; then
                echo "Did not install, curl is needed"
                return 1
        fi

        curl -L https://yt-dl.org/downloads/latest/youtube-dl -o /usr/local/bin/youtube-dl
        chmod a+rx /usr/local/bin/youtube-dl
        apt-get install libav-tools -y
fi

# install music player
if [ -f /etc/systemd/system/musicApp.service ]; then
        systemctl stop musicApp.service
fi
if [ -f /etc/systemd/system/musicWeb.service ]; then
        systemctl stop musicWeb.service
fi
if [ -d /root/musicPlayer ]; then
        rm -r /root/musicPlayer/ >/dev/null
        if [ $? -ne 0 ]; then
                echo "Unable to delete existing files"
                return 1
        fi
fi

mkdir -p /root/cache
mkdir -p /root/musicPlayer

cp /mnt/remote/myApps/musicPlayer /root/musicPlayer/
cp -r /mnt/remote/myApps/music-player-nodejs-copy /root/musicPlayer/nodejs
cp /mnt/remote/myApps/music-player-services/musicApp.service /etc/systemd/system/
cp /mnt/remote/myApps/music-player-services/musicWeb.service /etc/systemd/system/

# set services to start during startup
systemctl enable musicApp.service
systemctl enable musicWeb.service

# set up routing table for multicasting (default multicasting IP, uses ethernet)
# might need to have a command in C app if we're still allowing custom multicast IPs
route | grep "224.255.255.255" >/dev/null
if [ $? -eq 1 ]; then
        ip route add 224.0.0.0/4 dev eth0
fi

echo "Installation completed successfully!"
echo "You may reboot your device now to start the services"
