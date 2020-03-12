/*
 *   =========================================================================
 *    Copyright (C) 2014 - 2017 Eaton
 *
 *    This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *   =========================================================================
 */

/*
 * File:   avahi_wrapper.h
 *
 */

#ifndef AVAHI_WRAPPER_H
#define AVAHI_WRAPPER_H

#include <iostream>
#include <sstream>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <vector>
#include <string>
#include <mutex>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-client/lookup.h>

#include "fty_mdns_sd_classes.h"

#define SERVICE_NAME_KEY      "name"
#define SERVICE_TYPE_KEY      "type"
#define SERVICE_SUBTYPE_KEY   "subType"
#define SERVICE_PORT_KEY      "port"

typedef std::map<std::string, std::string> map_string_t;

void avahi_wrapper_test (bool verbose);

struct AvahiService {
    AvahiService(AvahiProtocol p, const std::string &n, const std::string &t, const std::string &d);

    AvahiProtocol protocol;
    std::string name, type, domain;
};

struct AvahiResolvedService {
    AvahiResolvedService(const AvahiService &s, const AvahiAddress *a, uint16_t p, const std::string &h, AvahiStringList *ptrtxt);

    AvahiService service;
    std::string address;
    uint16_t port;
    std::string hostname;
    std::vector<std::string> txt;
};


using AvahiResolvedServices = std::vector<AvahiResolvedService>;

class AvahiWrapper {
protected:
    //friend class AvahiGroupWrapper;

    /**
     * All class variable for the service.
     */
    map_string_t _serviceDefinition;
    char* _serviceName;
    AvahiStringList* _txtRecords;

    /**
     * All class variable to handle the avahi client object.
     */
    AvahiSimplePoll* _simplePoll;
    AvahiClient* _client;
    AvahiEntryGroup* _group;

    AvahiSimplePoll* _scanPoll;
    AvahiClient* _scanClient;
    AvahiServiceBrowser* _serviceBrowser;
    int _servicesToResolve;
    bool _doneBrowsing;
    bool _failed;
    AvahiResolvedServices _resolvedServices;
    std::mutex _scanMutex;

    std::string buildServiceName(const std::string &service_name,const std::string &uuid);
    AvahiEntryGroup* createService(AvahiClient* client,char* serviceName,map_string_t &serviceDefinition,AvahiStringList *txtRecords);

    bool isFinished() const {
        return (_doneBrowsing && (_servicesToResolve == 0)) || _failed;
    }

public:
    AvahiWrapper();
    ~AvahiWrapper();

    void setServiceDefinition(
        const std::string& service_name,
        const std::string& service_type,
        const std::string& service_stype,
        const std::string& port);

    char *getServiceName() { return _serviceName; };
    AvahiEntryGroup *getGroup() { return _group; };

    void clearTxtRecords();
    void setTxtRecord(const char* key, const char*value);
    void setTxtRecords(map_string_t &map);
    void setTxtRecords(zhash_t *map);

    void setHostName(const std::string& name);
    void printError(const std::string& msg, const char* errorNo);
    int start();
    void stop();
    void update();

    AvahiResolvedServices scanServices(const std::string &type);

    void clientCallback(AvahiClient* client);
    void resolveCallback(AvahiResolverEvent event, const AvahiResolvedService &service);
    void resolveFailureCallback(AvahiResolverEvent event);
    void browseCallback(AvahiBrowserEvent event, AvahiIfIndex interface, AvahiProtocol protocol, const AvahiService &service);
    void browseDoneCallback(AvahiBrowserEvent event);
    void browseFailureCallback(AvahiBrowserEvent event);
    void scanClientCallback(AvahiClientState event);
};
#endif
