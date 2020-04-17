# fty-mdns-sd

## Introduction:

The goal of this document is to describe the service announcement (mDNS) and devices discovery (DNS-SD) in IPM2 solution.

## Description:

The project repository is in github.com/42ity and is called fty-mdn-sd (https://github.com/42ity/fty-mdns-sd). It used standard architecture for compilation and CI developed for FTY Eaton component.

The project used the libavahi-client-dev client library (LGPL) based on apple bonjour project for Multicast DNS Service Discovery.
It used the new common message bus library for publication of discovery results on the malamute message bus.

The project has two modes:
* When the program is started as a service (default mode), it ensures the service announcement and the discovery of all devices on a local network.
* When the program is executed on standalone mode (--scan option), it discovers the devices on the local network and stops immediately after discovering.

In the two case, the discovery results are sent on the local message bus and/or the output standard.
The data formats for results are JSON and ASCII Text.

## Build the project
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

## Program options:
|Option   | Description |
|---------| ----------|
|--verbose (-v) | Verbose output |
|--help (-h) | Help information |
|--config (-c) | Path to config file |
|--endpoint (-e) | Malamute endpoint (ipc://@/malamute) |
|--daemonscan (-d)|  	Activate scan daemon |
|--autoscan (-a) |Activate automatic scan |
|--scan (-s) | Scan devices and quit (standalone mode) |
|--topic (-t) | Topic name for scan devices result |
|--stdout (-o) | Display scan devices result on standard output |
|--nopublishbus (-n) | Not publish scan devices result on malamute bus |

## Config file:
To configure the application, a configuration file exists: fty-mdn-sd.cfg. it is localized in the Linux standard path (/etc/fty-mdn-sd).

Example:
```
#   fty-mdns-sd configuration
server
    verbose = true                         # to setup verbose

fty-info
    name = fty-info                        # malamute info
    command = INFO                         # malamute command info

malamute
    endpoint = ipc://@/malamute            # malamute endpoint
    address = fty-mdns-sd                  # agent mdns-sd address

scan
    daemon_active = false                  # discover daemon activation
    auto = true                            # scan auto activation
    std_out = false                        # display result on standard output
    no_bus_out = false                     # no publication of result on bus
    command = START-SCAN                   # default command name for discover
    new_scan_topic = SCAN-NEW-ANNOUNCE     # default topic name for discover new devices
    default_scan_topic = SCAN-ANNOUNCE     # default topic name for devices scan
    type = _https._tcp                     # type of device filter
    sub_type = ups,pdu,ats                 # sub type of device filter
    manufacturer =                         # manufacturer device filter
    filter_key =                           # specific device filter (key)
    filter_value =                         # specific device filter (value)

log
    config = /etc/fty/ftylog.cfg           # config log
```

## Prerequisite:

Some daemons as fty-info and avahi-daemon are mandatory for fty-mdn-sd execution:

Verify execution of daemon withs following commands:
```bash
systemctl status fty-info
systemctl status avahi-daemon
```

Execute daemons if not running:
```bash
systemctl start fty-info
systemctl start avahi-daemon
```

## Execution of the program:

In service mode:
```bash
systemctl start fty-mdns-sd
```
In standalone mode with config file:
```bash
fty-mdn-sd -c fty-mdn-sd.cfg
```
In standalone mode (results on bus only):
```bash
fty-mdn-sd --scan
```
In standalone mode (results on standard output only):
```bash
fty-mdn-sd --scan --stdout --nopublishbus
```
## Service announcement:

When the service is started, the service announcement is automatically executed 5 seconds after the program start.
It sends INFO request to fty-info agent via message bus and sets default service definition and TXT properties via avahi.

In addition to that, agent is subscribed to ANNOUNCE stream (special stream where up-to-date INFO messages are periodically published).

On each INFO message, agent updates service definition and TXT properties, and publishes them to mDNS-SD via avahi.

Note, the announcement can be started manually with a command send directly on the internal message bus. It is a publish message on the specific topic ANNOUNCE with INFO (START-ANNOUNCE TBD ???) command.

```bash
fty-msgbus-cli publish -x -t ANNOUNCE INFO              -> Start a service announcement.
```

## Service discover:

### Manual scan

If the scan daemon option (--daemonscan) is activated, the program can discover devices on the network with a manual request. The results can be obtained in two different ways:

* Publish mode:

The request is a message published on the specific topic ANNOUNCE with START-SCAN command.
The scan results will be sent on another specific topic SCAN-ANNOUNCE.
The topic for results can be changed in config file (scan/default_scan_topic) or directly in the option (--topic) of the command line.
The results format is JSON.

```bash
fty-msgbus-cli publish -x -t ANNOUNCE START-SCAN   -> Start manual scan
fty-msgbus-cli publish -x -t ANNOUNCE START-SCAN MY-SCAN-ANNOUNCE  -> Start manual scan and results will be read on the  MY-SCAN-ANNOUNCE.
fty-msgbus-cli subscribe -t SCAN-ANNOUNCE          -> Read scan results on SCAN-ANNOUNCE .
fty-msgbus-cli subscribe -t MY-SCAN-ANNOUNCE       -> Read scan results on MY-SCAN-ANNOUNCE .
```

* Request/reply mode:

The result can be obtained also with a request/reply message on the topic REQUEST_ANNOUNCE.
In this case, the result is obtained directly in the response of the message.
The response is first the status (OK/ERROR) following by the results in JSON format if no error.
If an error occurred, the result is a string which describe the error.

```bash
fty-msgbus-cli synchRequest -d fty-mdns-sd -s REQUEST_ANNOUNCE -q REQUEST_ANNOUNCE START-SCAN
or with legacy message:
bmsg request fty-mdns-sd REQUEST_ANNOUNCE REQUEST 1234 START-SCAN
```

### Automatic mode

If automatic scan option is activated (--autoscan), all devices on the network are discovered at started and all new devices starting on the network will be automatically discovered during execution.
The result will be sent on the specific topic SCAN-NEW-ANNOUNCE.
The topic name can be changed in the config file (scan/new_scan_topic).
The results format is JSON.

```bash
fty-msgbus-cli subscribe -t SCAN-NEW-ANNOUNCE -> Read scan new devices on SCAN-NEW-ANNOUNCE
```

## Scan filter:

The service can discover all devices which implement mDNS/DNS-SD protocol on a local network. But it is possible to filter the devices during discover.
For that, there are several filters defined in the config file.
* scan/type: Type of device. e.g _https._tcp
* scan/sub-type: Sub-type of device. e.g ups, pdu or ats
* scan/manufacturer: Manufacturer of device. e.g eaton.
* scan/filter_key and scan/filter_value: Possibility to filter with a specific key and a specific value defined in the extended mapping data of the device. e.g scan/filter_key=vendor and scan/filter_value=HPE for filter vendor=HPE.

The default scan filter is:
* type = _https._tcp
* sub_type = ups,pdu,ats
* manufacturer = <empty>
* filter_key/value = <empty>

## Architecture

fty-mdns-sd has several parts:

* fty-mdns-sd-server: Actor for Malamute connection.
* fty-mdn-sd-manager: Manager of application.
* fty-mdn-sd-mapping: Mapping with JSON data.
* avahi-wrapper: Wrapper to avahi library.

## Data format:

For scan service request on the message bus, the result data format is JSON.
For standard output, the data format is ASCII Txt (key/value), each device is separated by “*” characters.

Example for JSON format:
```bash
[
    {
        "service_host_name": "ups-00-20-85-E9-48-FB.local",
        "service_name": "Gigabit Network Card (580488e4)",
        "service_address": "10.130.35.90",
        "service_port": "443",
        "service_extended_info":
        {
            "uuid": "580488e426a85057a95fa2ca82bce4d5",
            "vendor": "EATON",
            "manufacturer": "Eaton",
            "product": "3000i RT 3U",
            "serialNumber": "GA15G32059",
            "name": "",
            "path": "/etn/v1/comm",
            "type": "UPS",
            "location": "",
            "contact": "",
            "protocol-format": "etn"
        }
    },
    {
        "service_host_name": " ups-60-64-05-F6-96-A1-2.local",
        "service_name": "Gigabit Network Card (5922e43f) #2",
        "service_address": "10.130.33.7",
        "service_port": "443",
        "service_extended_info":
        {
            "uuid": "5922e43f10935853a5dc5ffd56bf15a8",
            "vendor": "HPE",
            "manufacturer": "Eaton",
            "product": "UPS LI R 1500",
            "serialNumber": "G117E22030",
            "name": "",
            "path": "/etn/v1/comm",
            "type": "UPS",
            "location": "",
            "contact": "",
            "protocol-format": "etn"
        }
    }
]
```

Example for TXT format:
```bash
service.hostname=ups-00-20-85-E9-48-FB.local
service.name=Gigabit Network Card (580488e4)
service.address=10.130.35.90
service.port=443
uuid=580488e426a85057a95fa2ca82bce4d5
vendor=EATON
manufacturer=Eaton
product=3000i RT 3U
serialNumber=GA15G32059
name
path=/etn/v1/comm
type=UPS
location
contact
protocol-format=etn
******************************
service.hostname=ups-60-64-05-F6-96-A1-2.local
service.name=Gigabit Network Card (5922e43f) #2
service.address=10.130.33.7
service.port=443
uuid=5922e43f10935853a5dc5ffd56bf15a8
vendor=HPE
manufacturer=Eaton
product=UPS LI R 1500
serialNumber=G117E22030
name
path=/etn/v1/comm
type=UPS
location
contact
protocol-format=etn
```

For JSON format, there is a DTO for data serialization/deserialization. There are the classes ServiceDeviceMapping and ServiceDeviceListMapping defined in the fty_mdns_sd_mapping files.
```
Serialization/deserialization classes:
map<string, string> ExtendedInfoMapping;
class ServiceDeviceMapping
{
    string m_serviceHostname;
    string m_serviceName;
    string m_serviceAddress;
    string m_servicePort;
    ExtendedInfoMapping m_extendedInfo;
}

list<ServiceDeviceMapping> ServiceDeviceList;
class ServiceDeviceListMapping
{
    ServiceDeviceList m_serviceDeviceList;
}
```
