/*  =========================================================================
    fty_mdns_sd_server - 42ity mdns sd server

    Copyright (C) 2014 - 2017 Eaton

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

#ifndef FTY_MDNS_SD_SERVER_H_INCLUDED
#define FTY_MDNS_SD_SERVER_H_INCLUDED

#include "fty_mdns_sd_manager.h"
#include "../src/avahi_wrapper.h"

class MdnsSdManager;

class MdnsSdServer
{
    public:
        struct Parameters
        {
            Parameters();

            std::string actorName;
            std::string endpoint;
            std::string requesterName;
            std::string requesterTempScanName;
            std::string requesterNewScanName;
            std::string ftyInfoCommand;
            std::string scanCommand;
            std::string scanDefaultTopic;
            std::string scanNewTopic;
            uint threadPoolSize;
        };

        MdnsSdServer(const Parameters parameters, MdnsSdManager &manager);
        ~MdnsSdServer() = default;

        int pollFtyInfo();

        Parameters getParameters() const { return m_parameters; };
        MdnsSdManager &getManager() const { return m_manager; };

        void publishMsgNewServices();
        void publishMsgServices(AvahiResolvedServices& avahiServices);
        void publishMsgServicesOnTopic(AvahiResolvedServices& avahiServices, const std::string &scanTopic);

    protected:

        /**
         * \brief Handle request assets message.
         * \param msg Message receive.
         */

        void handleNotifyService(messagebus::Message msg);
        void handleRequestService(messagebus::Message msg, std::string subject);

        const Parameters m_parameters;
        MdnsSdManager& m_manager;

        messagebus::PoolWorker m_worker;
        std::unique_ptr<messagebus::MessageBus> m_msgBusReceiver;
        std::unique_ptr<messagebus::MessageBus> m_msgBusRequester;
        std::unique_ptr<messagebus::MessageBus> m_msgBusRequesterNewScan;
};

//  Self test of this class
void fty_mdns_sd_server_test (bool verbose);

#endif
