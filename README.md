# fty-mdns-sd

Manages network announcement(mDNS) and discovery (DNS-SD) by collecting
information from fty-info agent, then publishing it through avahi-deamon

limitation: discovery is not yet managed

## How to build

To build, run:
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=usr -DBUILD_TESTING=On ..
make
sudo make install
```

## How to run

To run fty-mdns-sd project:

* from within the source tree, run:

```bash
./build/agent/fty-mdns-sd
```

For the other options available, refer to the manual page of fty-mdns-sd

* from an installed base, using systemd, run:

```bash
systemctl start fty-info
systemctl start avahi-daemon
systemctl start fty-mdns-sd
```

### Configuration file

The default configuration values are in fty-mdns-sd.cfg file.

* section server:
    * verbose - sets verbosity of the agent

* section fty-info
    * name - sets name of fty-info agent
    * command - sets command sent to fty-info agent

* section malamute: standard directives

## Architecture

### Overview

fty-mdns-sd has 1 actor:

* fty-mdns-sd-server: main actor

## Protocols

### Published metrics

Agent doesn't publish any metrics.

### Published alerts

Agent doesn't publish any alerts.

### Mailbox requests

Agent doesn't accept any mailbox requests.

### Stream subscriptions

On startup, agent send INFO request to fty-info agent, and sets default service definition and TXT properties via avahi.

In addition to that, agent is subscribed to ANNOUNCE stream (special stream where up-to-date INFO messages are periodically published).

On each INFO message, agent updates service definition and TXT properties, and publishes them to mDNS-SD via avahi.
