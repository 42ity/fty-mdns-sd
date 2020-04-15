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

/*
@header
    fty_mdns_sd_server - 42ity mdns sd server
@discuss
@end
*/

#include "fty_mdns_sd_classes.h"
//#include "fty_common_macros.h"  // TBD

#include <chrono>
#include <thread>
#include <bits/unique_ptr.h>

MdnsSdServer::Parameters::Parameters() :
    requesterName("fty-mdns-sd-requester"),
    requesterTempScanName("fty-mdns-sd-requester-tmp-scan"),
    requesterNewScanName("fty-mdns-sd-requester-new-scan"),
    threadPoolSize(3)
{
}

MdnsSdServer::MdnsSdServer(const Parameters parameters, MdnsSdManager &manager) :
    m_parameters(parameters),
    m_manager(manager),
    m_worker(m_parameters.threadPoolSize),
    m_msgBusReceiver(messagebus::MlmMessageBus(m_parameters.endpoint, m_parameters.actorName)),
    m_msgBusRequester(messagebus::MlmMessageBus(m_parameters.endpoint, m_parameters.requesterName))
{
    m_msgBusReceiver->connect();
    m_msgBusReceiver->subscribe("ANNOUNCE", std::bind(&MdnsSdServer::handleNotifyService, this, std::placeholders::_1));
    m_msgBusReceiver->receive("REQUEST_ANNOUNCE", std::bind(&MdnsSdServer::handleRequestService, this, std::placeholders::_1, "REQUEST_ANNOUNCE"));

    m_msgBusRequester->connect();

    if (m_manager.getParameters().scanAuto) {
        m_msgBusRequesterNewScan = std::unique_ptr<messagebus::MessageBus>(messagebus::MlmMessageBus(m_parameters.endpoint, m_parameters.requesterNewScanName));
        m_msgBusRequesterNewScan->connect();
    }
}

//  --------------------------------------------------------------------------
//  get info from fty-info agent

int MdnsSdServer::pollFtyInfo()
{
    try {
        messagebus::Message message;

        std::string uuid = messagebus::generateUuid();
        message.userData().push_back("INFO");
        message.userData().push_back(uuid);

        message.metaData().clear();
        message.metaData().emplace(messagebus::Message::RAW, "");
        message.metaData().emplace(messagebus::Message::SUBJECT, "info");
        message.metaData().emplace(messagebus::Message::FROM, m_parameters.requesterName);
        message.metaData().emplace(messagebus::Message::TO, "fty-info");
        message.metaData().emplace(messagebus::Message::CORRELATION_ID, uuid);
        message.metaData().emplace(messagebus::Message::REPLY_TO, m_parameters.requesterName);
        //m_msgBusRequester.get()->sendRequest("fty-info", message);
        messagebus::Message resp = m_msgBusRequester->request("fty-info", message, 10);
        /*for (const auto &data : resp.userData()) {
            log_info("data=\"%s\"", data.c_str());
        }
        for (const auto &meta : resp.metaData()) {
            log_info("meta:\"%s=%s\"", meta.first.c_str(), meta.second.c_str());
        }*/
        if (resp.userData().size() < 7)  {
            throw std::runtime_error("pollFtyInfoAndAnnounce: bad message");
        }
        log_trace("resp.userData().size()=%d", resp.userData().size());
        std::string uuid_recv = resp.userData().front();
        resp.userData().pop_front();
        if (uuid_recv != uuid) {
            throw std::runtime_error("pollFtyInfoAndAnnounce: receive bad uuid");
        }
        resp.userData().pop_front();  // pop subject

        // this suppose to be an update, service must be created already
        m_manager.setSrvName(resp.userData().front());
        resp.userData().pop_front();
        m_manager.setSrvType(resp.userData().front());
        resp.userData().pop_front();
        m_manager.setSrvSubType(resp.userData().front());
        resp.userData().pop_front();
        m_manager.setSrvPort(resp.userData().front());
        resp.userData().pop_front();
        log_debug("fty-mdns-sd-server: new ANNOUNCEMENT from %s", m_manager.getSrvName().c_str());
        // Get extended values
        std::string mystring = resp.userData().front();
        resp.userData().pop_front();
        zframe_t *frame_infos = zframe_new(mystring.c_str(), mystring.length());
        zhash_t *infos = zhash_unpack(frame_infos);
        std::map<std::string, std::string> map_txt;
        char *value = (char *) zhash_first(infos);
        while (value) {
            const char *key = (char *) zhash_cursor(infos);
            map_txt.insert(std::make_pair(key, value));
            value = (char *) zhash_next(infos);
        }
        m_manager.setMapTxt(map_txt);
        zhash_destroy(&infos);
        zframe_destroy(&frame_infos);
    } catch (messagebus::MessageBusException& ex) {
        log_error("%s", ex.what());
    }
    return 0;
}

void MdnsSdServer::publishMsgNewServices()
{
    // If some new services present
    if (m_manager.getService()->getNumberResolvedNewService() > 0) {
        while (m_manager.getService()->getNumberResolvedNewService() > 0) {
            AvahiResolvedService avahiNewService = m_manager.getService()->getLastResolvedNewService();
            log_trace("New service '%s' discover: count_service=%d", avahiNewService.service.name.c_str(), m_manager.addCountService());

            ServiceDeviceMapping serviceDeviceMapping = ServiceDeviceMapping::convertAvahiService(avahiNewService);
            messagebus::Message message;
            message.userData().push_back(serviceDeviceMapping.toString());
            message.metaData().clear();
            message.metaData().emplace(messagebus::Message::FROM, m_parameters.requesterName);
            //message.metaData().emplace(messagebus::Message::SUBJECT, "");
            if (m_msgBusRequesterNewScan != nullptr) m_msgBusRequesterNewScan->publish(m_parameters.scanNewTopic, message);
            if (m_manager.getParameters().scanStdOut) {
                m_manager.outputService(avahiNewService);
            }
        }
    }
}

void MdnsSdServer::publishMsgServices(AvahiResolvedServices& avahiServices)
{
    ServiceDeviceListMapping serviceDeviceListMapping = ServiceDeviceListMapping::convertAvahiServices(avahiServices);
    messagebus::Message message;
    message.userData().push_back(serviceDeviceListMapping.toString());
    message.metaData().emplace(messagebus::Message::FROM, m_parameters.requesterName);
    //message.metaData().emplace(messagebus::Message::SUBJECT, "");
    m_msgBusRequester->publish(m_parameters.scanDefaultTopic, message);
}

void MdnsSdServer::publishMsgServicesOnTopic(AvahiResolvedServices& avahiServices, const std::string &scanTopic)
{
    ServiceDeviceListMapping serviceDeviceListMapping = ServiceDeviceListMapping::convertAvahiServices(avahiServices);
    messagebus::Message message;
    message.userData().push_back(serviceDeviceListMapping.toString());
    message.metaData().emplace(messagebus::Message::FROM, m_parameters.requesterName);
    //message.metaData().emplace(messagebus::Message::SUBJECT, "");
    std::unique_ptr<messagebus::MessageBus> msgBusRequester(messagebus::MlmMessageBus(m_parameters.endpoint, m_parameters.requesterTempScanName));
    msgBusRequester->connect();
    msgBusRequester->publish(scanTopic, message);
}

void MdnsSdServer::handleNotifyService(messagebus::Message msg)
{
    log_trace("MdnsSdServer::handleNotifyService: received message.");
    m_worker.offload([this](messagebus::Message msg) {
        try {
            log_info("msg.userData().size()=%d", msg.userData().size());
            if (msg.userData().size() == 0)  {
                throw std::runtime_error("handleNotifyService: bad message");
            }
            std::string command = msg.userData().front();
            msg.userData().pop_front();
            //
            if (command == "INFO") {
                // this suppose to be an update, service must be created already
                m_manager.setSrvName(msg.userData().front());
                msg.userData().pop_front();
                m_manager.setSrvType(msg.userData().front());
                msg.userData().pop_front();
                m_manager.setSrvSubType(msg.userData().front());
                msg.userData().pop_front();
                m_manager.setSrvPort(msg.userData().front());
                msg.userData().pop_front();
                log_debug("fty-mdns-sd-server: new ANNOUNCEMENT from %s", m_manager.getSrvName().c_str());
                // Get extended values
                std::string mystring = msg.userData().front();
                msg.userData().pop_front();
                zframe_t *frame_infos = zframe_new(mystring.c_str(), mystring.length());
                zhash_t *infos = zhash_unpack(frame_infos);
                std::map<std::string, std::string> map_txt;
                char *value = (char *) zhash_first(infos);
                while (value) {
                    const char *key = (char *) zhash_cursor(infos);
                    map_txt.insert(std::make_pair(key, value));
                    value = (char *) zhash_next(infos);
                }
                m_manager.setMapTxt(map_txt);
                zhash_destroy(&infos);
                zframe_destroy(&frame_infos);
                m_manager.getService()->update();
            }
            // Scan services
            else if (command == m_parameters.scanCommand) {
                if (m_manager.getParameters().scanDaemonActive) {
                    auto results = m_manager.getService()->scanServices(m_manager.getParameters().scanType);
                    // publish result on bus if not deactivated
                    if (!m_manager.getParameters().scanNoPublishBus) {
                        std::string scanTopic;
                        std::string scanDefaultTopic = m_parameters.scanDefaultTopic;
                        if (msg.userData().size() >= 1) {
                            scanTopic = msg.userData().front();
                            msg.userData().pop_front();
                            log_debug("handleNotifyService: scanTopic=%s", scanTopic.c_str());
                        }
                        if (!scanTopic.empty() && scanDefaultTopic != scanTopic) {
                            publishMsgServicesOnTopic(results, scanTopic);
                        }
                        else {
                            publishMsgServices(results);
                        }
                    }
                    // display result on stdout if activated
                    if (m_manager.getParameters().scanStdOut) {
                        m_manager.outputServices(results);
                    }
                }
                else {
                    log_info("handleNotifyService: scan not activated");
                }
            }
            else {
                log_error("handleNotifyService: Unknown command %s", command.c_str());
            }
        }
        catch (std::exception& e) {
            log_error("Exception while processing notify service: %s", e.what());
        }
    }, std::move(msg));
}

void MdnsSdServer::handleRequestService(messagebus::Message msg, std::string subject)
{
    log_trace("MdnsSdServer::handleRequestService: received message.");

    m_worker.offload([this, subject](messagebus::Message msg) {
        try {
            log_debug("msg.userData().size()=%d msg.metaData().size()=%d", msg.userData().size(), msg.metaData().size());
            std::string uuid_recv;
            std::string message_type;
            // Note: Consider that it is a legacy message (which come from legacy message bus library) when only "_from" meta data is present
            bool isLegacyMessage = (msg.metaData().size() == 1 && msg.metaData().find(messagebus::Message::FROM) != msg.metaData().end());
            // If legacy message
            if (isLegacyMessage) {
                if (msg.userData().size() < 3)  {
                    throw std::runtime_error("handleRequestService: bad message");
                }
                message_type = msg.userData().front();
                msg.userData().pop_front();
                uuid_recv = msg.userData().front();
                msg.userData().pop_front();
            }
            // else new message
            else {
                if (msg.userData().size() < 1)  {
                    throw std::runtime_error("handleRequestService: bad message");
                }
                auto it = msg.metaData().find(messagebus::Message::CORRELATION_ID);
                if (it != msg.metaData().end()) {
                    uuid_recv = it->second;
                }
                else {
                    throw std::runtime_error("handleRequestService: no uuid");
                }
            }
            std::string command = msg.userData().front();
            msg.userData().pop_front();

            messagebus::Message response;
            // If legacy message, add mandatory user data
            if (isLegacyMessage) {
                response.userData().push_back(uuid_recv);
                response.userData().push_back("REPLY");  // TBD: To add in all case ???
            }
            response.userData().push_back(command);

            if (isLegacyMessage && message_type != "REQUEST") {
                log_error("handleRequestService: Invalid message type (%s)", message_type.c_str());
                response.userData().push_back("ERROR"); // status
                // TBD: translation
                //message.userData().push_back(TRANSLATE_ME("Invalid message type"));
                response.userData().push_back("Invalid message type");
            }
            else if (!m_manager.getParameters().scanDaemonActive) {
                log_error("handleRequestService: scan not activated");
                response.userData().push_back("ERROR"); // status
                // TBD: translation
                //message.userData().push_back(TRANSLATE_ME("Scan not activated"));
                response.userData().push_back("Scan not activated");
            }
            else if (command != m_parameters.scanCommand) {
                log_error("handleRequestService: Invalid command type (%s)", command.c_str());
                response.userData().push_back("ERROR"); // status
                // TBD: translation
                //response.userData().push_back(TRANSLATE_ME("Invalid command"));
                response.userData().push_back("Invalid command");
            }
            else {
                response.userData().push_back("OK"); // status
                auto results = m_manager.getService()->scanServices(m_manager.getParameters().scanType);
                // display result on stdout if activated
                if (m_manager.getParameters().scanStdOut) {
                    m_manager.outputServices(results);
                }
                ServiceDeviceListMapping serviceDeviceListMapping = ServiceDeviceListMapping::convertAvahiServices(results);
                response.userData().push_back(serviceDeviceListMapping.toString());
            }
            std::string replyQueue;
            // If legacy message, the reply queue is the subject and no send meta information to replier
            if (isLegacyMessage) {
                replyQueue = subject;
                response.metaData().emplace(messagebus::Message::RAW, "");
            }
            else {
                log_debug("subject=%s", subject.c_str());
                response.metaData().emplace(messagebus::Message::SUBJECT, subject);
                auto it = msg.metaData().find(messagebus::Message::REPLY_TO);
                if (it != msg.metaData().end()) {
                    log_info("read replyQueue=%s", it->second.c_str());
                    replyQueue = it->second;
                }
            }
            auto it = msg.metaData().find(messagebus::Message::FROM);
            if (it != msg.metaData().end()) {
                log_debug("Add meta:\"%s=%s\"", messagebus::Message::TO.c_str(), it->second.c_str());
                response.metaData().emplace(messagebus::Message::TO.c_str(), it->second);
            }
            response.metaData().emplace(messagebus::Message::CORRELATION_ID, uuid_recv);
            log_debug("replyQueue=%s", replyQueue.c_str());
            m_msgBusRequester->sendReply(replyQueue, response);
        }
        catch (std::exception& e) {
            log_error("Exception while processing request service: %s", e.what());
        }
    }, std::move(msg));
}

//  --------------------------------------------------------------------------
//  Self test of this class

void fty_mdns_sd_server_test (bool verbose)
{
    printf ("fty mdns sd server test: OK\n");
}
