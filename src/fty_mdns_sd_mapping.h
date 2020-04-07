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

#ifndef FTY_MDNS_SD_MAPPING_H_INCLUDED
#define FTY_MDNS_SD_MAPPING_H_INCLUDED

#include "cxxtools/serializationinfo.h"

/**
 * Some key definition for serialization
 *
 */

static constexpr const char* SERVICE_HOST_NAME_ENTRY = "service_host_name";
static constexpr const char* SERVICE_NAME_ENTRY = "service_name";
static constexpr const char* SERVICE_ADDRESS_ENTRY = "service_address";
static constexpr const char* SERVICE_PORT_ENTRY = "service_port";
static constexpr const char* SERVICE_EXTENDED_INFO_ENTRY = "service_extended_info";
static constexpr const char* SERVICE_DEVICES_LIST_ENTRY = "service_devices_list";

using ExtendedInfoMapping = std::map<std::string, std::string>;

/**
 * \brief ServiceDeviceMapping: Public interface
 */
class ServiceDeviceMapping
{
public:
    explicit ServiceDeviceMapping();
    ServiceDeviceMapping(std::string hostname, std::string name, std::string address, std::string port, ExtendedInfoMapping extendedInfo);

    void fillSerializationInfo(cxxtools::SerializationInfo& si) const;
    void fromSerializationInfo(const cxxtools::SerializationInfo& si);

    std::string toString() const;

    // Attributs
    std::string m_serviceHostname;
    std::string m_serviceName;
    std::string m_serviceAddress;
    std::string m_servicePort;
    ExtendedInfoMapping m_extendedInfo;
};

void operator<<= (cxxtools::SerializationInfo& si, const ServiceDeviceMapping & mapping);
void operator>>= (const cxxtools::SerializationInfo& si, ServiceDeviceMapping & mapping);
void operator>>= (const std::string& str, ServiceDeviceMapping & mapping);

// add a stream operator to display the ServiceDeviceMapping in debug for example
std::ostream& operator<< (std::ostream& os, const ServiceDeviceMapping & mapping);

using ServiceDeviceList = std::list<ServiceDeviceMapping>;

/**
 * \brief ServiceDeviceListMapping: Public interface
 */
class ServiceDeviceListMapping
{
public:
    explicit ServiceDeviceListMapping();
    ServiceDeviceListMapping(ServiceDeviceList serviceDevicesList);

    void fillSerializationInfo(cxxtools::SerializationInfo& si) const;
    void fromSerializationInfo(const cxxtools::SerializationInfo& si);

    std::string toString() const;

    // Attributs
    ServiceDeviceList m_serviceDeviceList;
};

void operator<<= (cxxtools::SerializationInfo& si, const ServiceDeviceListMapping & mapping);
void operator>>= (const cxxtools::SerializationInfo& si, ServiceDeviceListMapping & mapping);
void operator>>= (const std::string& str, ServiceDeviceMapping & mapping);

// add a stream operator to display the ServiceDevicesMapping in debug for example
std::ostream& operator<< (std::ostream& os, const ServiceDeviceListMapping & mapping);

//  Self test of this class
void fty_mdns_sd_mapping_test (bool verbose);

#endif
