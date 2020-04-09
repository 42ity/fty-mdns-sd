/*  =========================================================================
    fty_mdns_sd_mapping - 42ity mdns sd mapping class

    Copyright (C) 2019 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

#include "fty_mdns_sd_classes.h"

#include <cxxtools/jsonserializer.h>
#include <cxxtools/jsondeserializer.h>

//  --------------------------------------------------------------------------
// ServiceDeviceMapping
//

ServiceDeviceMapping::ServiceDeviceMapping():
    m_serviceHostname(),
    m_serviceName(),
    m_serviceAddress(),
    m_servicePort(),
    m_extendedInfo()
{}

ServiceDeviceMapping::ServiceDeviceMapping(const std::string &hostname, const std::string &name,
    const std::string &address, const std::string &port, const ExtendedInfoMapping &extendedInfo) :
    m_serviceHostname(hostname),
    m_serviceName(name),
    m_serviceAddress(address),
    m_servicePort(port),
    m_extendedInfo(extendedInfo)
{}

void ServiceDeviceMapping::fillSerializationInfo(cxxtools::SerializationInfo& si) const
{
    try {
        si.addMember(SERVICE_HOST_NAME_ENTRY) <<= m_serviceHostname;
        si.addMember(SERVICE_NAME_ENTRY) <<= m_serviceName;
        si.addMember(SERVICE_ADDRESS_ENTRY) <<= m_serviceAddress;
        si.addMember(SERVICE_PORT_ENTRY) <<= m_servicePort;
        auto& extendedSi = si.addMember(SERVICE_EXTENDED_INFO_ENTRY);
        extendedSi.setCategory(cxxtools::SerializationInfo::Object);
        for (auto &info : m_extendedInfo) {
            extendedSi.addMember(info.first) <<= info.second;
        }
    }
    catch(const std::exception& e) {
        log_error("Error in json: %s", e.what());
    }
}

void ServiceDeviceMapping::fromSerializationInfo(const cxxtools::SerializationInfo& si)
{
    try {
        si.getMember(SERVICE_HOST_NAME_ENTRY) >>= m_serviceHostname;
        si.getMember(SERVICE_NAME_ENTRY) >>= m_serviceName;
        si.getMember(SERVICE_ADDRESS_ENTRY) >>= m_serviceAddress;
        si.getMember(SERVICE_PORT_ENTRY) >>= m_servicePort;
        auto &extended = si.getMember(SERVICE_EXTENDED_INFO_ENTRY);
        for (auto &member : extended) {
            std::string key = member.name();
            std::string value;
            member.getValue(value);
            m_extendedInfo.insert(make_pair(key, value));
        }
    }
    catch(const std::exception& e) {
        log_error("Error in json: %s", e.what());
    }
}

std::string ServiceDeviceMapping::toString() const
{
    std::string returnData("");

    try {
        cxxtools::SerializationInfo si;
        fillSerializationInfo(si);

        std::stringstream output;
        cxxtools::JsonSerializer serializer(output);
        serializer.beautify(true);
        serializer.serialize(si);
        returnData = output.str();
    }
    catch(const std::exception& e) {
        log_error("Error in json: %s", e.what());
    }
    return returnData;
}

ServiceDeviceMapping ServiceDeviceMapping::convertAvahiService(AvahiResolvedService const& avahiResolvedService)
{
    ServiceDeviceMapping serviceDeviceMapping;
    serviceDeviceMapping.m_serviceHostname = avahiResolvedService.hostname;
    serviceDeviceMapping.m_serviceName = avahiResolvedService.service.name;
    serviceDeviceMapping.m_serviceAddress = avahiResolvedService.address;
    std::ostringstream s;
    s << avahiResolvedService.port;
    serviceDeviceMapping.m_servicePort = s.str();
    ExtendedInfoMapping extendedInfo;
    for (auto &elemt : avahiResolvedService.txt) {
        std::string::size_type pos;
        if ((pos = elemt.find("=")) != std::string::npos) {
            std::string key = elemt.substr(0, pos);
            std::string value = elemt.substr(pos + 1);
            extendedInfo.insert(std::make_pair(key, value));
        }
    }
    serviceDeviceMapping.m_extendedInfo = extendedInfo;
    return serviceDeviceMapping;
}

void operator>>= (const std::string& str, ServiceDeviceMapping& mapping)
{
    cxxtools::SerializationInfo si;

    try {
        std::stringstream input;
        input << str;
        cxxtools::JsonDeserializer deserializer(input);
        deserializer.deserialize(si);
    }
    catch(const std::exception& e) {
        log_error("Error in json: %s", e.what());
    }

    mapping.fromSerializationInfo(si);
}

void operator<<= (cxxtools::SerializationInfo& si, const ServiceDeviceMapping& mapping)
{
    mapping.fillSerializationInfo(si);
}

void operator>>= (const cxxtools::SerializationInfo& si, ServiceDeviceMapping& mapping)
{
    mapping.fromSerializationInfo(si);
}

// add a stream operator to display the ServiceDeviceMapping in debug for example
std::ostream& operator<< (std::ostream& os, const ServiceDeviceMapping& mapping)
{
    os << mapping.toString() << std::endl;
    return os;
}


//  --------------------------------------------------------------------------
// ServiceDeviceListMapping
//

ServiceDeviceListMapping::ServiceDeviceListMapping():
    m_serviceDeviceList()
{}

ServiceDeviceListMapping::ServiceDeviceListMapping(const ServiceDeviceList& serviceDeviceList) :
    m_serviceDeviceList(serviceDeviceList)
{}

void ServiceDeviceListMapping::fillSerializationInfo(cxxtools::SerializationInfo& si) const
{
    try {
        si.setCategory(cxxtools::SerializationInfo::Array);
        for (auto &service : m_serviceDeviceList) {
            si.addMember("") <<= service;
        }
    }
    catch(const std::exception& e) {
        log_error("Error in json: %s", e.what());
    }
}

void ServiceDeviceListMapping::fromSerializationInfo(const cxxtools::SerializationInfo& si)
{
    try {
        for (auto &service : si) {
            ServiceDeviceMapping serviceDeviceMapping;
            service >>= serviceDeviceMapping;
            m_serviceDeviceList.push_back(serviceDeviceMapping);
        }
    }
    catch(const std::exception& e) {
        log_error("Error in json: %s", e.what());
    }
}

std::string ServiceDeviceListMapping::toString() const
{
    std::string returnData("");

    try {
        cxxtools::SerializationInfo si;
        fillSerializationInfo(si);

        std::stringstream output;
        cxxtools::JsonSerializer serializer(output);
        serializer.beautify(true);
        serializer.serialize(si);

        returnData = output.str();
    }
    catch(const std::exception& e) {
        log_error("Error in json: %s", e.what());
    }
    return returnData;
}

ServiceDeviceListMapping ServiceDeviceListMapping::convertAvahiServices(AvahiResolvedServices const& avahiResolvedServices)
{
    ServiceDeviceListMapping serviceDeviceListMapping;
    for (auto &avahiResolvedService : avahiResolvedServices) {
        serviceDeviceListMapping.m_serviceDeviceList.push_back(ServiceDeviceMapping::convertAvahiService(avahiResolvedService));
    }
    return serviceDeviceListMapping;
}


void operator>>= (const std::string& str, ServiceDeviceListMapping & mapping)
{
    cxxtools::SerializationInfo si;

    try
    {
        std::stringstream input;
        input << str;
        cxxtools::JsonDeserializer deserializer(input);
        deserializer.deserialize(si);
    }
    catch(const std::exception& e)
    {
        log_error("Error in json: %s", e.what());
    }

    mapping.fromSerializationInfo(si);
}

void operator<<= (cxxtools::SerializationInfo& si, const ServiceDeviceListMapping & mapping)
{
    mapping.fillSerializationInfo(si);
}

void operator>>= (const cxxtools::SerializationInfo& si, ServiceDeviceListMapping & mapping)
{
    mapping.fromSerializationInfo(si);
}

// add a stream operator to display the ServiceDevicesMapping in debug for example
std::ostream& operator<< (std::ostream& os, const ServiceDeviceListMapping & mapping)
{
    os << mapping.toString() << std::endl;
    return os;
}


//  --------------------------------------------------------------------------
//  Self test of this class

void fty_mdns_sd_mapping_test (bool verbose)
{
    printf ("fty mdns sd mapping test\n");

    // ServiceDeviceMapping serialization/de-serialization
    {
        ServiceDeviceMapping serviceDeviceMapping;
        ServiceDeviceMapping serviceDeviceMappingInit("Host name", "Name", "Address", "Port", {{ "Key1", "Value1"}, { "Key2", "Value2"}});
        std::string strServiceDeviceMappingInit = serviceDeviceMappingInit.toString();
        std::cout << serviceDeviceMappingInit;
        strServiceDeviceMappingInit >>= serviceDeviceMapping;
        std::cout << serviceDeviceMapping;
        assert(strServiceDeviceMappingInit == serviceDeviceMapping.toString());
    }

    // ServiceDeviceListMapping serialization/de-serialization
    {
        ServiceDeviceListMapping serviceDeviceListMapping;
        ServiceDeviceListMapping serviceDeviceListMappingInit(
        {
            {"Host name 1", "Name 1", "Address 1", "Port 1", {{ "Key 1_1", "Value 1_1"}, { "Key 1_2", "Value 1_2"}}},
            {"Host name 2", "Name 2", "Address 2", "Port 2", {{ "Key 2_1", "Value 2_1"}, { "Key 2_2", "Value 2_2"}}}
        });
        std::string strServiceDeviceListMappingInit = serviceDeviceListMappingInit.toString();
        std::cout << serviceDeviceListMappingInit;
        strServiceDeviceListMappingInit >>= serviceDeviceListMapping;
        std::cout << serviceDeviceListMapping;
        assert(strServiceDeviceListMappingInit == serviceDeviceListMapping.toString());
    }
}

