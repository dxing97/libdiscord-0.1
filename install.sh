#!/bin/bash
if lsb_release -ds | grep "Ubuntu 17.04"; then
    echo Detected supported Ubuntu distribution
else
    echo Detected unsupported distribution! Continue? \(y/n\):
    read CONT
    if [ $CONT=y ]; then
	echo continuing
    else if [$CONT=n ]; then
	     echo exiting
	     exit
	 else
	     echo not an option, exiting
	     exit
	 fi
    fi
    
fi

exit
	 
sudo apt update && sudo apt upgrade
sudo apt install checkinstall libmicrohttpd-dev libjansson-dev libcurl4-gnutls-dev libgnutls28-dev libgcrypt20-dev git make cmake gcc libssl-dev 
cd ~/

#install ulfius
git clone https://github.com/babelouest/ulfius.git
cd ulfius/
git submodule update --init
cd lib/orcania
make && sudo checkinstall
cd ../yder
make && sudo checkinstall
cd ../..
make
sudo checkinstall
cd ..

#install libwebsockets
git clone https://github.com/warmcat/libwebsockets
cd libwebsockets
mkdir build
cd build 
cmake ..
make
sudo checkinstall --pkgname libwebsockets
sudo ldconfig

#compile client 
#gcc -lwebsockets -lssl -lcrypto test-client.c -lwebsockets -lssl -lcrypto
