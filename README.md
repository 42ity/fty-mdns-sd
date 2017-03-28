# fty-mdns-sd
Manages network announcement(mDNS) and discovery (DNS-SD) by collecting
information from fty-info agent, then publishing it through avahi-deamon

The default configuration values are in fty-mdns-sd.cfg file (section default)

limitation: discovery is not yet managed

## How to build
To build fty-mdns-sd project run:
```bash
sudo apt-get install fty-info
sudo apt-get install avahi-daemon
sudo apt-get install libavahi-client-dev
./autogen.sh [clean]
./configure
make
make check # to run self-test
```
