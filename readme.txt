sudo apt-get install gcc-multilib
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install libevent-dev:i386
cmake .
make
