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

//#include "avahi_wrapper.h"
#include "fty_mdns_sd_classes.h"
#include <czmq.h>

extern "C" void s_client_callback(AvahiClient* client, AvahiClientState state, void *userdata)
{
    try {
        if (userdata != nullptr) {
            AvahiWrapper* clientWrapper = reinterpret_cast<AvahiWrapper*>(userdata);
            switch (state) {

                case AVAHI_CLIENT_S_RUNNING:
                    log_debug("AVAHI_CLIENT_S_RUNNING");
                    /* The server has startup successfully and registered its host
                     * name on the network, so create our services */
                    clientWrapper->clientCallback(client);
                    break;

                case AVAHI_CLIENT_S_REGISTERING:
                    log_debug("AVAHI_CLIENT_S_REGISTERING");
                    if (clientWrapper->getGroup()) avahi_entry_group_reset(clientWrapper->getGroup());
                    break;
                case AVAHI_CLIENT_FAILURE:
                    log_error("AVAHI_CLIENT_FAILURE :%", avahi_strerror(avahi_client_errno(client)));
                    break;
                case AVAHI_CLIENT_S_COLLISION:
                    log_warning("AVAHI_CLIENT_S_COLLISION");
                    break;
                case AVAHI_CLIENT_CONNECTING:
                    log_debug("AVAHI_CLIENT_CONNECTING");
                    break;
                default:
                    break;
            }
        }
    }
    catch (std::exception& e) {
        log_error( "clientCallback exception:%s " , e.what() );
    }
}

extern "C" void s_group_callback(AvahiEntryGroup* group, AvahiEntryGroupState state, void *userdata)
{
    try {
        if (userdata != nullptr) {

            AvahiWrapper* clientWrapper = reinterpret_cast<AvahiWrapper*>(userdata);

            switch (state) {
                case AVAHI_ENTRY_GROUP_ESTABLISHED:
                    // The entry group has been established successfully.
                    log_debug("Service:'%s' successfully established.", clientWrapper->getServiceName());
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
        log_error(  "groupCallback exception: %s",e.what() );
    }
}

extern "C" void s_resolve_new_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *a,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    void *userdata) {
    AvahiWrapper *avahiWrapper = reinterpret_cast<AvahiWrapper*>(userdata);
    if (event == AVAHI_RESOLVER_FOUND) {
        avahiWrapper->resolveNewCallback(event, AvahiResolvedService(AvahiService(protocol, name, type, domain), a, port, host_name, txt));
    }
    else {
        log_error("Avahi resolve new failed for new service: %s", name);
    }
}

extern "C" void s_browse_new_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AvahiLookupResultFlags flags,
    void *userdata) {
    AvahiWrapper *avahiWrapper = reinterpret_cast<AvahiWrapper*>(userdata);
    switch (event) {
        case AVAHI_BROWSER_FAILURE:
            log_error("Avahi browse new failed for new service: %s", name);
            break;
        case AVAHI_BROWSER_NEW:
            avahiWrapper->browseNewCallback(event, interface, protocol, AvahiService(protocol, name, type, domain));
            break;
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
        default:
            break;
    }
}

extern "C" void s_resolve_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *a,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    void *userdata) {
    AvahiWrapper *avahiWrapper = reinterpret_cast<AvahiWrapper*>(userdata);
    if (event == AVAHI_RESOLVER_FOUND) {
        avahiWrapper->resolveCallback(event, AvahiResolvedService(AvahiService(protocol, name, type, domain), a, port, host_name, txt));
    }
    else {
        avahiWrapper->resolveFailureCallback(event);
    }
}

extern "C" void s_browse_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AvahiLookupResultFlags flags,
    void *userdata) {
    AvahiWrapper *avahiWrapper = reinterpret_cast<AvahiWrapper*>(userdata);
    switch (event) {
        case AVAHI_BROWSER_FAILURE:
            avahiWrapper->browseFailureCallback(event);
            break;
        case AVAHI_BROWSER_NEW:
            avahiWrapper->browseCallback(event, interface, protocol, AvahiService(protocol, name, type, domain));
            break;
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            avahiWrapper->browseDoneCallback(event);
        default:
            break;
    }
}

extern "C" void s_client_scan_callback(AvahiClient *client, AvahiClientState state, void *userData) {
    AvahiWrapper *avahiWrapper = reinterpret_cast<AvahiWrapper*>(userData);
    avahiWrapper->scanClientCallback(state);
}

// TBD: To put in a seperate file
std::string str_tolower(const std::string &s) {
    std::string res(s);
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c){ return std::tolower(c); }
                  );
    return res;
}

std::vector<std::string> str_split(const std::string &str, char sep) {
    std::vector<std::string> res;
    std::string::size_type start = 0, end;
    while ((end = str.find(sep, start)) != std::string::npos) {
        res.push_back(str.substr(start, end - start));
        start = end + 1;
        if (start > str.size()) {
            start = str.size();
        }
    }
    res.push_back(str.substr(start));
    return res;
}

AvahiResolvedService::AvahiResolvedService(const AvahiService &s, const AvahiAddress *a, uint16_t p, const std::string &h, AvahiStringList *t) :
    service(s), address(), port(p), hostname(h) {
    char buffer[AVAHI_ADDRESS_STR_MAX];
    avahi_address_snprint(buffer, sizeof(buffer), a);
    address = buffer;

    for (AvahiStringList *iterTxt = t; iterTxt; iterTxt = iterTxt->next) {
        txt.emplace_back(reinterpret_cast<const char*>(iterTxt->text), iterTxt->size);
    }
}

AvahiService::AvahiService(AvahiProtocol p, const std::string &n, const std::string &t, const std::string &d) :
    protocol(p), name(n), type(t), domain(d) {
}

AvahiScanFilter::AvahiScanFilter(const std::string &s, const std::string &m, const std::string &k, const std::string &v) :
    sub_type(s), manufacturer(m), filter_key(k), filter_value(v)
{
}

AvahiWrapper::AvahiWrapper() :
    _serviceName(nullptr),
    _txtRecords(nullptr),
    _simplePoll(nullptr),
    _client(nullptr),
    _group(nullptr),
    _scanPoll(nullptr),
    _scanClient(nullptr),
    _serviceBrowser(nullptr),
    _scanFilter(),
    _servicesToResolve(0),
    _failed(false)
{
}

AvahiWrapper::~AvahiWrapper()
{
    // Free all resources.
    stop();
    if (_txtRecords) avahi_string_list_free(_txtRecords);
    if (_serviceName) avahi_free(_serviceName);
}

std::string
AvahiWrapper::buildServiceName(const std::string &service_name,const std::string &uuid)
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
    log_info("avahi_string_list_add(TXT %s)",(_key + "=" + _value).c_str());

}

void AvahiWrapper::setTxtRecords(const map_string_t &map)
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

    // Allocate main loop object
    if (!(_simplePoll = avahi_simple_poll_new())) {
        throw std::runtime_error("Couldn't allocate avahi simple poller.");
    }
    if (!(_client = avahi_client_new(avahi_simple_poll_get(_simplePoll), static_cast<AvahiClientFlags>(0), s_client_callback, this, &error))) {
        throw std::runtime_error(avahi_strerror(error));
    }
    return error;
}

void AvahiWrapper::stop()
{
    if (_group) {
        avahi_entry_group_reset( _group );
        avahi_entry_group_free( _group );
    }
    if (_serviceBrowser) avahi_service_browser_free(_serviceBrowser);
    if (_client) avahi_client_free(_client);
    if (_simplePoll) avahi_simple_poll_free(_simplePoll);
    _group = nullptr;
    _serviceBrowser = nullptr;
    _client = nullptr;
    _simplePoll = nullptr;
}

void AvahiWrapper::printError(const std::string& msg, const char* errorNo)
{
    log_error("avahi error %s %s", msg.c_str(), errorNo);
    avahi_simple_poll_quit(_simplePoll);
}

AvahiEntryGroup* AvahiWrapper::createService(AvahiClient* client, char* serviceName, map_string_t &serviceDefinition, AvahiStringList *txtRecords)
{
    AvahiEntryGroup *group;
    assert(client);
    int rv;
    group = avahi_entry_group_new(client, s_group_callback, this);
    if(!group){
        log_error("avahi_entry_group_new() failed", avahi_strerror(avahi_client_errno(client)));
        //throw NullPointerException("Cannot create service group");
        return nullptr;
    }
    // The group is empty (either because it was just created
    if (avahi_entry_group_is_empty(group)) {
        log_info("Adding service: %s,%s,%d," ,
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
            log_error( "Service name collision, renaming service from:%s to:%s", serviceName, n);
            avahi_free(_serviceName);
            serviceName = n;
            avahi_entry_group_reset(group);
            return createService(client,serviceName,serviceDefinition,txtRecords); //potential deadlock :(
        }
        // Add subtype
        log_info("Adding subtype: %s,%s,%s",
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
            log_error("Failed to add subtype: %s, %s" ,
                    serviceDefinition[SERVICE_SUBTYPE_KEY].c_str(),
                    avahi_strerror(rv));
            throw std::runtime_error("AFailed to add subtype:");
        }
        // Tell the server to register the service.
        rv = avahi_entry_group_commit(group);
        if (rv<0){
            log_error("Failed to commit entry group: %s" ,
                    avahi_strerror(rv));
            throw std::runtime_error("Registering Avahi services failed");
        }
        log_info( "Service added" );
    }
    else {
        log_info("Group not empty");
    }
    return group;
}

void AvahiWrapper::announce()
{
    _group = createService(_client, _serviceName, _serviceDefinition, _txtRecords);
}

void AvahiWrapper::update()
{
    if (_group == NULL) {
        log_warning ("Update called but service doesnt exist yet!");
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
        log_error("Failed to update service: %s", avahi_strerror (rv));
    }
}

void AvahiWrapper::startBrowseNewServices(std::string type)
{
    if (!(_serviceBrowser = avahi_service_browser_new(_client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, type.c_str(),
        nullptr, static_cast<AvahiLookupFlags>(0), s_browse_new_callback, this))) {
        throw std::runtime_error("Couldn't allocate avahi service browser.");
    }
}

int AvahiWrapper::waitEvents()
{
    int res = avahi_simple_poll_iterate(_simplePoll, TIMEOUT_WAIT_EVENTS);
    return res;
}

AvahiResolvedService AvahiWrapper::getLastResolvedNewService()
{
    std::lock_guard<std::mutex> lockNewServiceMutex(_newServiceMutex);
    AvahiResolvedService avahiNewService = _resolvedNewServices.back();
    _resolvedNewServices.pop_back();
    return avahiNewService;
}

void AvahiWrapper::addResolvedNewService(const AvahiResolvedService &avahiNewService)
{
    // Test if service is a not filter
    if (!isFilterService(avahiNewService.txt)) {
        std::lock_guard<std::mutex> lockNewServiceMutex(_newServiceMutex);
        _resolvedNewServices.insert(_resolvedNewServices.begin(), avahiNewService);
    }
    else {
        log_info("Filter service (%s, %s) during scan", avahiNewService.service.name.c_str(), avahiNewService.hostname.c_str());
    }
}

void AvahiWrapper::setScanFilter(const AvahiScanFilter &scanFilter) {
    _scanFilter.sub_type = scanFilter.sub_type;
    _scanFilter.manufacturer = scanFilter.manufacturer;
    _scanFilter.filter_key = scanFilter.filter_key;
    _scanFilter.filter_value = scanFilter.filter_value;
}

bool AvahiWrapper::isFilterService(const std::vector<std::string> &key_value_service)
{
    bool res = false;
    for (const auto& filter : std::vector<std::pair<std::string, std::function<bool(const std::string&)>>> ({
        // Sub type filter treatment
        { "type", [this](std::string value) -> bool
            {
                if (this->_scanFilter.sub_type.empty()) {
                    return false;
                }
                bool r = true;
                std::vector<std::string> sub_types = str_split(this->_scanFilter.sub_type, ',');
                for (const auto &sub_type : sub_types) {
                    if (str_tolower(value) == str_tolower(sub_type)) {
                        r = false;
                        break;
                    }
                }
                return r;
            }
        },
        // Manufacturer filter treatment
        { "manufacturer", [this](std::string value) -> bool
            {
                return (this->_scanFilter.manufacturer.empty() || str_tolower(value) == str_tolower(this->_scanFilter.manufacturer)) ? false : true;
            }
        },
        // Specific key/value filter treatment
        { this->_scanFilter.filter_key, [this](std::string value) -> bool
            {
                return (this->_scanFilter.filter_value.empty() || value == this->_scanFilter.filter_value) ? false : true;
            }
        }
    }))
    {
        if (!filter.first.empty()) {
            bool key_find = false;
            for (const auto &key_value_txt : key_value_service) {
                std::vector<std::string> key_value = str_split(key_value_txt, '=');
                if (key_value.size() > 1) {
                    std::string key = key_value[0];
                    std::string value = key_value[1];
                    if (filter.first == key) {
                        key_find = true;
                        res = filter.second(value);
                        break;
                    }
                }
            }
            if (!key_find) res = true;
            if (res) {
                break;
            }
        }
    }
    return res;
}

void AvahiWrapper::clientCallback(AvahiClient* client)
{
    try {
        assert(client);
    }
    catch (std::exception& e) {
        log_error( "onClientRunning exception: %s" , e.what() );
    }
}

AvahiResolvedServices AvahiWrapper::scanServices(const std::string &type)
{
    std::lock_guard<std::mutex> lockScanMutex(_scanMutex);

    if (!(_scanPoll = avahi_simple_poll_new())) {
        throw std::runtime_error("Couldn't allocate avahi scan poller.");
    }
    int error;
    if(!(_scanClient = avahi_client_new(avahi_simple_poll_get(_scanPoll), static_cast<AvahiClientFlags>(0), s_client_scan_callback, this, &error))) {
        throw std::runtime_error(avahi_strerror(error));
    }
    if (!(_serviceBrowser = avahi_service_browser_new(_scanClient, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, type.c_str(), nullptr, static_cast<AvahiLookupFlags>(0), s_browse_callback, this))) {
        throw std::runtime_error(avahi_strerror(error));
    }
    avahi_simple_poll_loop(_scanPoll);
    avahi_service_browser_free(_serviceBrowser);
    avahi_client_free(_scanClient);
    avahi_simple_poll_free(_scanPoll);

    _serviceBrowser = nullptr;
    _scanPoll = nullptr;
    _servicesToResolve = 0;
    _doneBrowsing = false;
    if (_failed) {
        _failed = false;
        throw std::runtime_error("Failure while using avahi.");
    }

    return std::move(_resolvedServices);
}

void AvahiWrapper::resolveNewCallback(AvahiResolverEvent event, const AvahiResolvedService &service)
{
    addResolvedNewService(service);
}

void AvahiWrapper::browseNewCallback(AvahiBrowserEvent event, AvahiIfIndex interface, AvahiProtocol protocol, const AvahiService &service)
{
    if (!(avahi_service_resolver_new(_client, interface, service.protocol, service.name.c_str(), service.type.c_str(), service.domain.c_str(),
        AVAHI_PROTO_UNSPEC, static_cast<AvahiLookupFlags>(0), s_resolve_new_callback, this))) {
        log_error("Avahi browse failed for new service: %s", service.name.c_str());
    }
}

void AvahiWrapper::resolveCallback(AvahiResolverEvent event, const AvahiResolvedService &service)
{
    // Test if service is a not filter
    if (!isFilterService(service.txt)) {
        _resolvedServices.push_back(service);
    }
    else {
        log_info("Filter service (%s, %s) during scan", service.service.name.c_str(), service.hostname.c_str());
    }
    _servicesToResolve--;

    if (isFinished()) {
        avahi_simple_poll_quit(_scanPoll);
    }
}

void AvahiWrapper::resolveFailureCallback(AvahiResolverEvent event)
{
    _servicesToResolve--;

    if (isFinished()) {
        avahi_simple_poll_quit(_scanPoll);
    }
}

void AvahiWrapper::browseCallback(AvahiBrowserEvent event, AvahiIfIndex interface, AvahiProtocol protocol, const AvahiService &service)
{
    if (!(avahi_service_resolver_new(_scanClient, interface, service.protocol, service.name.c_str(), service.type.c_str(), service.domain.c_str(),
        AVAHI_PROTO_UNSPEC, static_cast<AvahiLookupFlags>(0), s_resolve_callback, this))) {
        _failed = true;
        avahi_simple_poll_quit(_scanPoll);
    }
    else {
        _servicesToResolve++;
    }
}

void AvahiWrapper::browseDoneCallback(AvahiBrowserEvent event)
{
    _doneBrowsing = true;
    if (isFinished()) {
        avahi_simple_poll_quit(_scanPoll);
    }
}

void AvahiWrapper::browseFailureCallback(AvahiBrowserEvent event)
{
    _failed = true;
    avahi_simple_poll_quit(_scanPoll);
}

void AvahiWrapper::scanClientCallback(AvahiClientState event)
{
    if (event == AVAHI_CLIENT_FAILURE) {
        _failed = true;
        avahi_simple_poll_quit(_simplePoll);
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void avahi_wrapper_test (bool verbose)
{
    printf ("Avahi wrapper test\n");

    // String test
    {
        assert(str_tolower("") == "");
        assert(str_tolower("AAAA") == "aaaa");

        std::vector<std::string> res;
        res = str_split("", ',');
        assert(res.size() == 1);
        assert(res[0] == "");

        res = str_split("key", '=');
        assert(res.size() == 1);
        assert(res[0] == "key");

        res = str_split("key=", '=');
        assert(res.size() == 2);
        assert(res[0] == "key");
        assert(res[1] == "");

        res = str_split("aaa,bbb,ccc", ',');
        assert(res.size() == 3);
        assert(res[0] == "aaa");
        assert(res[1] == "bbb");
        assert(res[2] == "ccc");
    }

    // AvahiScanFilter wrapper test
    {
        AvahiWrapper avahiWrapper;
        const auto &configs = std::vector<AvahiScanFilter> {
            AvahiScanFilter("", "", "", ""),
            AvahiScanFilter("ups", "", "", ""),
            AvahiScanFilter("ups,pdu", "", "", ""),
            AvahiScanFilter("ups,pdu", "EATON", "", ""),
            AvahiScanFilter("ups,pdu", "EATON", "key", ""),
            AvahiScanFilter("ups,pdu", "EATON", "key", "value")
        };

        const auto &expectedResults = std::vector<std::pair<std::vector<std::string>, std::array<bool, 6>>> {
            { { "type=ups", "manufacturer=EATON", "key=value" },
              { false, false, false, false, false, false}
            },
            { { "type=ops", "manufacturer=EATON", "key=value" },
              { false, true, true, true, true, true}
            },
            { { "type=ups", "manufacturer=OTHER", "key=value" },
              { false, false, false, true, true, true}
            },
            { { "type=ups", "manufacturer=EATON", "key=other" },
              { false, false, false, false, false, true}
            }
        };

        for (size_t i = 0; i < expectedResults.size(); i++) {
            for (size_t j = 0; j < configs.size(); j++) {
                avahiWrapper.setScanFilter(configs[j]);
                std::cout << "Test " << i << "/" << j << std::endl;
                assert(avahiWrapper.isFilterService(expectedResults[i].first) == expectedResults[i].second[j]);
            }
        }
    }

#if 0
    // AvahiWrapper wrapper test
    {
        auto results = wrapper.scanServices("_https._tcp");
        for (const auto &result : results) {
            std::cout << std::string(30, '*') << std::endl;
            std::cout << "service.hostname=" << result.hostname << std::endl;
            std::cout << "service.name=" << result.service.name << std::endl;
            std::cout << "service.address=" << result.address << std::endl;
            std::cout << "service.port=" << result.port << std::endl;
            for (const auto &txt : result.txt) {
                std::cout << txt << std::endl;
            }
        }
    }
#endif

}
