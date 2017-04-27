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


#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "fty_mdns_sd_classes.h"

#define SERVICE_NAME_KEY      "name"
#define SERVICE_TYPE_KEY      "type"
#define SERVICE_SUBTYPE_KEY   "subType"
#define SERVICE_PORT_KEY      "port"

typedef std::map<std::string, std::string> map_string_t;

void avahi_wrapper_test (bool verbose);

class AvahiWrapper {
protected:
    //friend class AvahiGroupWrapper;

    /**
     * All class variable for the service.
     */
    map_string_t _serviceDefinition;
    char* _serviceName = nullptr;
    AvahiStringList* _txtRecords = nullptr;

    std::string getServiceName(const std::string &service_name,const std::string &uuid);
    AvahiEntryGroup* create_service(AvahiClient* client,char* serviceName,map_string_t &serviceDefinition,AvahiStringList *txtRecords);

    /**
     * All class variable to handle the avahi client object.
     */
    AvahiSimplePoll* _simplePoll = nullptr;
    AvahiClient* _client = nullptr;
    AvahiEntryGroup* _group = nullptr;

public:

    ~AvahiWrapper();

    void setServiceDefinition(
        const std::string& service_name,
        const std::string& service_type,
        const std::string& service_stype,
        const std::string& port);

    void clearTxtRecords();
    void setTxtRecord(const char* key, const char*value);
    void setTxtRecords(map_string_t &map);
    void setTxtRecords(zhash_t *map);

    void setHostName(const std::string& name);

    void printError(const std::string& msg, const char* errorNo);

    int start();

    void stop();

    void update();

protected:

    void onClientRunning(AvahiClient* client);

    static void clientCallback(AvahiClient* client, AvahiClientState state, void *userdata);

    static void groupCallback(AvahiEntryGroup* group, AvahiEntryGroupState state, void *userdata);
};
#endif
