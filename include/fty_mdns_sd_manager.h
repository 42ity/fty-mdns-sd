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

#ifndef FTY_MDNS_SD_MANAGER_H_INCLUDED
#define FTY_MDNS_SD_MANAGER_H_INCLUDED

#include "fty_mdns_sd_server.h"
#include "../src/avahi_wrapper.h"

class MdnsSdManager
{
    public:
        struct Parameters
        {
            Parameters();

            bool scanOnly;
            bool scanDaemonActive;
            bool scanAuto;
            bool scanStdOut;
            bool scanNoPublishBus;

            std::string scanType;
            std::string scanSubType;
            std::string scanManufacturer;
            std::string scanFilterKey;
            std::string scanFilterValue;
        };

        MdnsSdManager(Parameters parameters);
        ~MdnsSdManager() = default;

        AvahiWrapper *getService() { return m_service.get(); };
        Parameters getParameters() { return m_parameters; };

        std::string getSrvName() { return m_srvName; };
        std::string getSrvType() { return m_srvType; };
        std::string getSrvSubType() { return m_srvSubType; };
        std::string getSrvPort() { return m_srvPort; };
        std::map<std::string, std::string> &getMapTxt() { return m_mapTxt; };

        void setSrvName(std::string srvName) { m_srvName = srvName; };
        void setSrvType(std::string srvType) { m_srvType = srvType; };
        void setSrvSubType(std::string srvSubType) { m_srvSubType = srvSubType; };
        void setSrvPort(std::string srvPort) { m_srvPort = srvPort; };
        void setMapTxt(std::map<std::string, std::string> mapTxt) { m_mapTxt = mapTxt; };

        void init(MdnsSdServer& server);
        //void setServer(MdnsSdServer& server) { m_server = &server; };
        void doDefaultAnnounce();
        void doDefaultAnnounce(const uint wait_sec);
        void doAnnounce();
        void doScan();

        static void outputService(const AvahiResolvedService& avahiService);
        static void outputServices(const AvahiResolvedServices& avahiServices);

        int addCountService() { return ++m_countService; };

    protected:
        std::string m_ftyInfoCommand;
        const Parameters m_parameters;
        MdnsSdServer *m_server;
        std::unique_ptr<AvahiWrapper> m_service;  // service mDNS-SD
        int m_countService;

        // default service announcement definition
        std::string m_srvName;
        std::string m_srvType;
        std::string m_srvSubType;
        std::string m_srvPort;
        //TXT attributes
        std::map<std::string, std::string> m_mapTxt;
};

//  Self test of this class
void fty_mdns_sd_manager_test (bool verbose);

#endif
