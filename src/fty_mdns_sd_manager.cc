/*  =========================================================================
    fty_mdns_sd_manager - 42ity mdns sd manager

    Copyright (C) 2014 - 2018 Eaton

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

/*
@header
    fty_mdns_sd_server - 42ity mdns sd server
@discuss
@end
*/

#include "fty_mdns_sd_classes.h"

MdnsSdManager::Parameters::Parameters()
{

}

MdnsSdManager::MdnsSdManager(Parameters parameters) :
    m_parameters(parameters),
    m_server(nullptr),
    m_service(new AvahiWrapper()),  // service mDNS-SD
    m_countService(0),
    m_srvName(DEFAULT_SERVICE_NAME),
    m_srvType(DEFAULT_SERVICE_TYPE),
    m_srvSubType(DEFAULT_SERVICE_SUB_TYPE),
    m_srvPort(DEFAULT_SERVICE_PORT)
{
    m_service->setScanFilter(AvahiScanFilter(
        m_parameters.scanSubType,
        m_parameters.scanManufacturer,
        m_parameters.scanFilterKey,
        m_parameters.scanFilterValue)
    );
}

void MdnsSdManager::init() {
    // Start Avahi
    getService()->start();

    // Start browsing new devices if scan auto is activated
    if (m_parameters.scanAuto) {
        getService()->startBrowseNewServices(m_parameters.scanType);
    }
}

void MdnsSdManager::outputService(const AvahiResolvedService& avahiService)
{
    std::cout << "service.hostname=" << avahiService.hostname << std::endl;
    std::cout << "service.name=" << avahiService.service.name << std::endl;
    std::cout << "service.address=" << avahiService.address << std::endl;
    std::cout << "service.port=" << avahiService.port << std::endl;
    for (const auto &txt : avahiService.txt) {
        std::cout << txt << std::endl;
    }
}

void MdnsSdManager::outputServices(const AvahiResolvedServices& avahiServices)
{
    for (const auto &avahiService : avahiServices) {
        std::cout << std::string(30, '*') << std::endl;
        outputService(avahiService);
    }
}

void MdnsSdManager::doDefaultAnnounce(const uint wait_sec) {
    // Do default service announcement after time specified
    // TBD: Need return future for working !!!
    /*auto f = std::async(std::launch::async, [this, wait_sec](){
        std::this_thread::sleep_for(std::chrono::seconds(wait_sec));
        doDefaultAnnounce();
	});*/
    std::thread t([this, wait_sec](){
        std::this_thread::sleep_for(std::chrono::seconds(wait_sec));
        doDefaultAnnounce();
	});
    t.detach();
}

void MdnsSdManager::doDefaultAnnounce()
{
    // Get announce parameters from fty-info agent (retry #3 before logging error)
    if (m_server) {
        int retry = 1;
        //int retry = 3;
        while (retry--) {
            // If no error, stop retry
            if (m_server->pollFtyInfo() == 0) {
                break;
            }
            // else wait a little before retry
            if (retry > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        if (retry < 0) {
            log_error("doDefaultAnnounce: impossible to get fty-info agent parameters");
        }
    }
    else {
        log_error("doDefaultAnnounce: server not defined");
    }

    // Do announce
    doAnnounce();
}

void MdnsSdManager::doAnnounce()
{
    getService()->setServiceDefinition(
        getSrvName(),
        getSrvType(),
        getSrvSubType(),
        getSrvPort());
    // set all txt properties
    getService()->setTxtRecords(getMapTxt());
    getService()->announce();
}

void MdnsSdManager::doScan()
{
    auto results = getService()->scanServices(m_parameters.scanType);
    // publish result on bus if not deactivated
    if (!m_parameters.scanNoPublishBus) {
        if (m_server) {
            m_server->publishMsgServices(results);
        }
        else {
            log_error("doScan: server not defined");
        }
    }
    // display result on stdout if activated
    if (m_parameters.scanStdOut) {
        outputServices(results);
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

//#define AVAHI_DAEMON_RUNNING
#undef AVAHI_DAEMON_RUNNING

void fty_mdns_sd_manager_test (bool verbose)
{
    printf ("fty mdns sd manager test:\n");

#ifdef AVAHI_DAEMON_RUNNING
    MdnsSdManager::Parameters mdnsSdManagerParameters;
    mdnsSdManagerParameters.scanStdOut = true;
    mdnsSdManagerParameters.scanNoPublishBus = true;
    mdnsSdManagerParameters.scanType = DEFAULT_SCAN_TYPE;
    mdnsSdManagerParameters.scanSubType = "ipm";
    mdnsSdManagerParameters.scanFilterKey = "";
    mdnsSdManagerParameters.scanFilterValue = "";

    // launch manager
    MdnsSdManager mdnsSdManager(mdnsSdManagerParameters);
    mdnsSdManager.init();

    zuuid_t *uuid = zuuid_new();
    std::string strUuid(zuuid_str_canonical(uuid));
    zuuid_destroy(&uuid);

    std::stringstream buffer;
    buffer << "IPM TEST (" << strUuid << ")";
    std::string name = buffer.str();
    mdnsSdManager.setSrvName(name);
    mdnsSdManager.setSrvType(DEFAULT_SCAN_TYPE);
    mdnsSdManager.setSrvSubType("_powerservice._sub._https._tcp.");
    mdnsSdManager.setMapTxt({{"type", "ipm"}, {"manufacturer", "eaton"}});

    // do default service announcement
    mdnsSdManager.doAnnounce();

    // redirect stdout for checking scan result
    buffer.str("");
    std::streambuf *backup = std::cout.rdbuf(buffer.rdbuf());

    mdnsSdManager.doScan();

    // restore stdout
    std::cout.rdbuf(backup);
    std::cout << buffer.str() << std::endl;

    // test if announcement test has been detected during scan
    // Note: test pass even if avahi daemon not running (local boucle)
    assert(buffer.str().find("service.name=" + name) != std::string::npos);
#endif

    printf ("fty mdns sd manager test: OK\n");
}



