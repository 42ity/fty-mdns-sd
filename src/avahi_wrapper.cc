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
 * File:   avahi_wrapper.cc
 * Author: Xavier Millieret <XavierMillieret@Eaton.com>
 *
 */

#include "avahi_wrapper.h"
#include <czmq.h>

AvahiWrapper::~AvahiWrapper()
{
    // Free all resources.
    stop();
    if (_txtRecords) avahi_string_list_free(_txtRecords);
    if (_serviceName) avahi_free(_serviceName);
}

std::string
AvahiWrapper::getServiceName(const std::string &service_name,const std::string &uuid)
{
    std::ostringstream buffer;
    buffer << service_name << " ("<< uuid.substr(0, uuid.find("-")) << ")";
    return buffer.str();
}

void AvahiWrapper::setServiceDefinition(
    const std::string& service_name,
    const std::string& service_type,
    const std::string& service_stype,
    const std::string& port)
{
    _serviceName = avahi_strdup( service_name.c_str());
    _serviceDefinition[SERVICE_TYPE_KEY]    = std::string(service_type);
    _serviceDefinition[SERVICE_SUBTYPE_KEY] = std::string(service_stype);
    _serviceDefinition[SERVICE_PORT_KEY]    = std::string(port);
}

void AvahiWrapper::clearTxtRecords()
{
    if (_txtRecords) avahi_string_list_free(_txtRecords);
    _txtRecords = NULL;
}

void AvahiWrapper::setTxtRecord(const char* key, const char*value)
{
    std::string _key = std::string(key);
    std::string _value = std::string(value);
    _txtRecords = avahi_string_list_add(_txtRecords, (_key + "=" + _value).c_str());
    zsys_info("avahi_string_list_add(TXT %s)",(_key + "=" + _value).c_str());

}

void AvahiWrapper::setTxtRecords(map_string_t &map)
{
    clearTxtRecords ();
    for (auto it: map) {
        setTxtRecord (it.first.c_str (), it.second.c_str ());
    }
}

void AvahiWrapper::setTxtRecords(zhash_t *map)
{
    if (!map) return;
    clearTxtRecords ();
    char *value = (char *) zhash_first (map);
    while (value) {
        const char *key = (char *) zhash_cursor (map);
        setTxtRecord (key, value);
        value = (char *) zhash_next (map);
    }
}


void AvahiWrapper::setHostName(const std::string& name)
{
    avahi_client_set_host_name(_client, name.c_str());
}

int AvahiWrapper::start()
{
    int error = 0;
    /* Allocate main loop object */
    if (!(_simplePoll = avahi_simple_poll_new())) {
        zsys_error( "Failed to create simple poll object.");
    }
    _client = avahi_client_new(avahi_simple_poll_get(_simplePoll), AvahiClientFlags(0), AvahiWrapper::clientCallback, this, &error);
    return error;
}

void AvahiWrapper::stop()
{
    if (_group) {
        avahi_entry_group_reset( _group );
        avahi_entry_group_free( _group );
    }
    if (_client) avahi_client_free(_client);
    if (_simplePoll) avahi_simple_poll_free(_simplePoll);
    _group = nullptr;
    _client = nullptr;
    _simplePoll=nullptr;


}

void AvahiWrapper::printError(const std::string& msg, const char* errorNo)
{
    zsys_error("avahi error %s %s", msg.c_str(), errorNo);
    avahi_simple_poll_quit(_simplePoll);
}


AvahiEntryGroup* AvahiWrapper::create_service(AvahiClient* client,char* serviceName,map_string_t &serviceDefinition,AvahiStringList *txtRecords)
{
    AvahiEntryGroup *group;
    assert(client);
    int rv;
    group = avahi_entry_group_new(client, AvahiWrapper::groupCallback, this);
    if(!group){
        zsys_error("avahi_entry_group_new() failed", avahi_strerror(avahi_client_errno(client)));
        //throw NullPointerException("Cannot create service group");
        return nullptr;
    }
    // The group is empty (either because it was just created
    if (avahi_entry_group_is_empty(group)) {
        zsys_info("Adding service: %s,%s,%d," ,
                serviceName,
                serviceDefinition[SERVICE_TYPE_KEY].c_str(),
                std::stoi(serviceDefinition[SERVICE_PORT_KEY].c_str()));
        rv = avahi_entry_group_add_service_strlst(group,
            AVAHI_IF_UNSPEC,
            AVAHI_PROTO_UNSPEC,
            AvahiPublishFlags(0),
            serviceName,
            serviceDefinition[SERVICE_TYPE_KEY].c_str(),
            nullptr,
            nullptr,
            std::stoi(serviceDefinition[SERVICE_PORT_KEY].c_str()),
            txtRecords);
        if (rv== AVAHI_ERR_COLLISION) {
            char *n = avahi_alternative_service_name(serviceName);
            zsys_error( "Service name collision, renaming service from:%s to:%s" ,serviceName,n );
            avahi_free(_serviceName);
            serviceName = n;
            avahi_entry_group_reset(group);
            return create_service(client,serviceName,serviceDefinition,txtRecords); //potential deadlock :(
        }
        // Add subtype
        zsys_info("Adding subtype: %s,%s,%s",
                serviceName,
                serviceDefinition[SERVICE_TYPE_KEY].c_str(),
                serviceDefinition[SERVICE_SUBTYPE_KEY].c_str());
        rv = avahi_entry_group_add_service_subtype(group,
                AVAHI_IF_UNSPEC,
                AVAHI_PROTO_UNSPEC,
                AvahiPublishFlags(0),
                serviceName,
                serviceDefinition[SERVICE_TYPE_KEY].c_str(),
                nullptr,
                serviceDefinition[SERVICE_SUBTYPE_KEY].c_str());
        if (rv<0){
            zsys_error("Failed to add subtype: %s, %s" ,
                    serviceDefinition[SERVICE_SUBTYPE_KEY].c_str(),
                    avahi_strerror(rv));
            throw std::runtime_error("AFailed to add subtype:");
        }
        // Tell the server to register the service.
        rv = avahi_entry_group_commit(group);
        if (rv<0){
            zsys_error("Failed to commit entry group: %s" ,
                    avahi_strerror(rv));
            throw std::runtime_error("Registering Avahi services failed");
        }
        zsys_info( "Service added" );
    }
    return group;
}

void AvahiWrapper::update()
{
    if (_group == NULL) {
        zsys_warning ("Update called but service doesnt exist yet!");
        return;
    }

    int rv = avahi_entry_group_update_service_txt_strlst(
        _group,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        AvahiPublishFlags(0),
        _serviceName,
        _serviceDefinition[SERVICE_TYPE_KEY].c_str(),
        nullptr, //domain
        _txtRecords);

    if (rv < 0) {
        zsys_error("Failed to update service: %s", avahi_strerror (rv));
    }
}


void AvahiWrapper::onClientRunning(AvahiClient* client)
{
    try {
        assert(client);
        _group = create_service(client,_serviceName,_serviceDefinition,_txtRecords);
    }
    catch (std::exception& e) {
        zsys_error( "onClientRunning exception: %s" , e.what() );
    }
}

void AvahiWrapper::clientCallback(AvahiClient* client, AvahiClientState state, void *userdata)
{
    try {
        if (userdata != nullptr) {
            AvahiWrapper* clientWrapper = (AvahiWrapper*) userdata;
            switch (state) {

                case AVAHI_CLIENT_S_RUNNING:
                    zsys_debug("AVAHI_CLIENT_S_RUNNING");
                    /* The server has startup successfully and registered its host
                     * name on the network, so create our services */
                    //createServices(client);
                    clientWrapper->onClientRunning(client);
                    break;

                case AVAHI_CLIENT_S_REGISTERING:
                    zsys_debug("AVAHI_CLIENT_S_REGISTERING");
                    if (clientWrapper->_group) avahi_entry_group_reset(clientWrapper->_group);
                    break;
                case AVAHI_CLIENT_FAILURE:
                    zsys_error("AVAHI_CLIENT_FAILURE :%", avahi_strerror(avahi_client_errno(client)));
                    break;
                case AVAHI_CLIENT_S_COLLISION:
                    zsys_warning("AVAHI_CLIENT_S_COLLISION");
                    break;
                case AVAHI_CLIENT_CONNECTING:
                    zsys_debug("AVAHI_CLIENT_CONNECTING");
                    break;
                default:
                    break;
            }
        }
    }
    catch (std::exception& e) {
        zsys_error( "clientCallback exception:%s " , e.what() );
    }
}

void AvahiWrapper::groupCallback(AvahiEntryGroup* group, AvahiEntryGroupState state, void *userdata)
{
    try {
        if (userdata != nullptr) {
            AvahiWrapper* clientWrapper = (AvahiWrapper*) userdata;

            switch (state) {
                case AVAHI_ENTRY_GROUP_ESTABLISHED:
                    // The entry group has been established successfully.
                    std::cout << "Service:'" << clientWrapper->_serviceName << "' successfully established." << std::endl;
                    break;
                case AVAHI_ENTRY_GROUP_COLLISION:
                    //TODO
                    //clientWrapper->serviceCollision(avahi_entry_group_get_client(group));
                    break;
                case AVAHI_ENTRY_GROUP_FAILURE:
                    clientWrapper->printError("Failed to commit entry group: ", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(group))));
                    break;

                case AVAHI_ENTRY_GROUP_UNCOMMITED:
                case AVAHI_ENTRY_GROUP_REGISTERING:
                    ;
            }

        }
    }
    catch (std::exception& e) {
        zsys_error(  "groupCallback exception: %s",e.what() );
    }
}

void avahi_wrapper_test (bool verbose) {
    printf ("Avahi wrapper test\n");
}
