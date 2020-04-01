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
#include "avahi_wrapper.h"

#define TIMEOUT_MS 5000   //wait at least 5 seconds

//  Structure of our class


struct _fty_mdns_sd_server_t {
    char *name;              // actor name
    mlm_client_t *client;    // malamute client
    char *fty_info_command;
    AvahiWrapper *service;   // service mDNS-SD
    int count_service;

    //default service announcement definition
    char *srv_name;
    char *srv_type;
    char *srv_stype;
    char *srv_port;
    //TXT attributes
    zhash_t *map_txt;

    char *scan_command;
    char *scan_topic;
    char *scan_new_topic;
    char *scan_type;
    bool scan_auto;
    bool scan_std_out;
    bool scan_no_publish_bus;

    std::mutex publishMessageMutex;
};

//free dynamic item
static void s_destroy_txt(void *arg)
{
    char *value=(char*)arg;
    zstr_free(&value);
}

static void
s_set_txt_record(fty_mdns_sd_server_t *self,const char *key,const char *value)
{
    if(value==NULL) return;
    log_debug ("s_set_txt_record(%s,%s)",key,value);
    char *_value = strdup(value);
    int rv=zhash_insert(self->map_txt,key,_value);
    if(rv==-1)
        zhash_update(self->map_txt,key,_value);
    zhash_freefn(self->map_txt,key,s_destroy_txt);
}

static void
s_set_txt_records(fty_mdns_sd_server_t *self, zhash_t * map_txt)
{
    if(map_txt==NULL) return;
    if(self->map_txt!=NULL)
        zhash_destroy (&self->map_txt);
    self->map_txt = zhash_dup(map_txt);
}

static void
s_set_srv_name(fty_mdns_sd_server_t *self,const char *value)
{
    if(value==NULL) return;
    //delete previous dyn value
    zstr_free (&self->srv_name);
    char *_value = strdup(value);
    self->srv_name = _value;
}

static void
s_set_srv_port(fty_mdns_sd_server_t *self,const char *value)
{
    if(value==NULL) return;
    //delete previous dyn value
    zstr_free (&self->srv_port);
    char *_value = strdup(value);
    self->srv_port = _value;
}

static void
s_set_srv_type(fty_mdns_sd_server_t *self,const char *value)
{
    if(value==NULL) return;
    //delete previous dyn value
    zstr_free (&self->srv_type);
    char *_value = strdup(value);
    self->srv_type = _value;
}

static void
s_set_srv_stype(fty_mdns_sd_server_t *self,const char *value)
{
    if(value==NULL) return;
    //delete previous dyn value
    zstr_free (&self->srv_stype);
    char *_value = strdup(value);
    self->srv_stype = _value;
}

static void
s_set_scan_command(fty_mdns_sd_server_t *self, const char *value)
{
    if (value == NULL) return;
    // delete previous value
    zstr_free(&self->scan_command);
    char *_value = strdup(value);
    self->scan_command = _value;
}

static void
s_set_scan_topic(fty_mdns_sd_server_t *self, const char *value)
{
    if (value == NULL) return;
    // delete previous value
    zstr_free(&self->scan_topic);
    char *_value = strdup(value);
    self->scan_topic = _value;
}

static void
s_set_scan_new_topic(fty_mdns_sd_server_t *self, const char *value)
{
    if (value == NULL) return;
    // delete previous value
    zstr_free(&self->scan_new_topic);
    char *_value = strdup(value);
    self->scan_new_topic = _value;
}

static void
s_set_scan_type(fty_mdns_sd_server_t *self,const char *value)
{
    if (value == NULL) return;
    // delete previous value
    zstr_free(&self->scan_type);
    char *_value = strdup(value);
    self->scan_type = _value;
}

//  --------------------------------------------------------------------------
//  Create a new fty_mdns_sd_server
fty_mdns_sd_server_t *
fty_mdns_sd_server_new (const char* name)
{
    fty_mdns_sd_server_t *self = (fty_mdns_sd_server_t *) zmalloc (sizeof (fty_mdns_sd_server_t));
    assert (self);
    //  Initialize class properties here
    if (!name) {
        log_error ("Address for fty_mdns_sd actor is NULL");
        free (self);
        return NULL;
    }
    self->name    = strdup (name);
    self->client  = mlm_client_new();
    self->service = new AvahiWrapper(); // service mDNS-SD
    self->count_service = 0;
    self->map_txt = zhash_new();

    //do minimal initialization
    s_set_txt_record(self, "uuid", "00000000-0000-0000-0000-000000000000");

    self->scan_command = NULL;
    self->scan_topic = NULL;
    self->scan_new_topic = NULL;
    self->scan_type = NULL;
    self->scan_auto = false;
    self->scan_std_out = false;
    self->scan_no_publish_bus = false;

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fty_mdns_sd_server

void
fty_mdns_sd_server_destroy (fty_mdns_sd_server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fty_mdns_sd_server_t *self = *self_p;
        //  Free class properties here
        zstr_free (&self->name);
        zstr_free (&self->srv_name);
        zstr_free (&self->srv_type);
        zstr_free (&self->srv_stype);
        zstr_free (&self->srv_port);
        zstr_free (&self->fty_info_command);
        mlm_client_destroy (&self->client);
        zhash_destroy (&self->map_txt);
        delete self->service;
        zstr_free (&self->scan_command);
        zstr_free (&self->scan_topic);
        zstr_free (&self->scan_new_topic);
        zstr_free (&self->scan_type);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  get info from fty-info agent

static int
s_poll_fty_info(fty_mdns_sd_server_t *self)
{
    assert (self);

    zmsg_t *send = zmsg_new ();
    zmsg_addstr (send, self->fty_info_command);
    zuuid_t *uuid = zuuid_new ();
    zmsg_addstr (send, zuuid_str_canonical (uuid));
    log_debug ("requesting fty-info ..");
    if (mlm_client_sendto(self->client, "fty-info", "info", NULL, 1000, &send) != 0)
    {
        log_error("info: client->sendto (address = '%s') failed.", "fty-info");
        zmsg_destroy(&send);
        zuuid_destroy(&uuid);
        return -2;
    }

    zmsg_t *resp = mlm_client_recv(self->client);
    if (!resp)
    {
        log_error ("info: client->recv (timeout = '5') returned NULL");
        return -3;
    }

    char *zuuid_or_error = zmsg_popstr (resp);
    assert(strneq (zuuid_or_error, "ERROR"));
    //TODO : check UUID if you think it is important
    zstr_free(&zuuid_or_error);
    zuuid_destroy(&uuid);

    char *cmd = zmsg_popstr (resp);
    if(strneq (cmd, "INFO")) {
        log_error ("%s: not received INFO command (%s)", __func__, cmd);
        return -4;
    }
    char *srv_name  = zmsg_popstr (resp);
    char *srv_type  = zmsg_popstr (resp);
    char *srv_stype = zmsg_popstr (resp);
    char *srv_port  = zmsg_popstr (resp);

    s_set_srv_name(self,srv_name);
    s_set_srv_type(self,srv_type);
    s_set_srv_stype(self,srv_stype);
    s_set_srv_port(self,srv_port);

    zframe_t *frame_infos = zmsg_next (resp);
    zhash_t *infos = zhash_unpack(frame_infos);
    s_set_txt_records(self,infos);

    zhash_destroy(&infos);
    zstr_free (&cmd);
    zstr_free (&srv_name);
    zstr_free (&srv_type);
    zstr_free (&srv_stype);
    zstr_free (&srv_port);
    zmsg_destroy(&resp);
    zmsg_destroy(&send);

    return 0;
}

void static
s_output_service(const AvahiResolvedService& avahiService)
{
    std::cout << "service.hostname=" << avahiService.hostname << std::endl;
    std::cout << "service.name=" << avahiService.service.name << std::endl;
    std::cout << "service.address=" << avahiService.address << std::endl;
    std::cout << "service.port=" << avahiService.port << std::endl;
    for (const auto &txt : avahiService.txt) {
        std::cout << txt << std::endl;
    }
}

void static
s_output_services(const AvahiResolvedServices& avahiServices)
{
    for (const auto &avahiService : avahiServices) {
        std::cout << std::string(30, '*') << std::endl;
        s_output_service(avahiService);
    }
}

void
publish_msg_new_services(fty_mdns_sd_server_t* self)
{
    if (!(self && self->client && self->service && self->scan_new_topic)) {
        log_error("publish_msg_new_service failed: bad input parameter");
        return;
    }

    // If some new services present
    if (self->service->getNumberResolvedNewService() > 0) {
        // Secure producer for publish (TBD)
        std::lock_guard<std::mutex> lockPublishMessageMutex(self->publishMessageMutex);
        if (mlm_client_set_producer(self->client, self->scan_new_topic) == -1) {
            log_error("%s:\tSet producer to '%s' failed", self->name, self->scan_new_topic);
            return;
        }

        while (self->service->getNumberResolvedNewService() > 0) {
            AvahiResolvedService avahiNewService = self->service->getLastResolvedNewService();
            log_trace("New service '%s' discover: count_service=%d", avahiNewService.service.name.c_str(), ++self->count_service);
            zmsg_t *msg_service = zmsg_new();
            for (const auto& data : std::vector<std::string>({
                std::string("service.hostname=") + avahiNewService.hostname,
                std::string("service.name=") + avahiNewService.service.name,
                std::string("service.address=") + avahiNewService.address,
                std::string("service.port=") + std::to_string(avahiNewService.port)
            })) {
                zmsg_addstr(msg_service, data.c_str());
            }
            for (const auto &txt : avahiNewService.txt) {
                zmsg_addstr(msg_service, txt.c_str());
            }
            int rc = mlm_client_send(self->client, "MESSAGE", &msg_service);
            if (rc != 0)
            {
                log_error("mlm_client_send failed (rc: %d)", rc);
            }
            if (self->scan_std_out) {
                s_output_service(avahiNewService);
            }
            zmsg_destroy(&msg_service);
        }
    }
}

void static
s_publish_msg_services(fty_mdns_sd_server_t* self, AvahiResolvedServices& avahiServices, const char *scan_topic)
{
    if (!(self && self->client && self->scan_topic)) {
        log_error("publish_msg_new_service failed: bad input parameter");
        return;
    }

    // Need to change producer if scan auto activated
    if (self->scan_auto) {
        // Secure producer for publish (TBD)
        std::lock_guard<std::mutex> lockPublishMessageMutex(self->publishMessageMutex);
        if (mlm_client_set_producer(self->client, scan_topic ? scan_topic : self->scan_topic) == -1) {
            log_error("%s:\tSet producer to '%s' failed", self->name, scan_topic ? scan_topic : self->scan_topic);
            return;
        }
    }
    for(const AvahiResolvedService avahiService : avahiServices)
    {
        zmsg_t *msg_service = zmsg_new();
        for (const auto& data : std::vector<std::string>({
            std::string("service.hostname=") + avahiService.hostname,
            std::string("service.name=") + avahiService.service.name,
            std::string("service.address=") + avahiService.address,
            std::string("service.port=") + std::to_string(avahiService.port)
        })) {
            zmsg_addstr(msg_service, data.c_str());
        }
        for (const auto &txt : avahiService.txt) {
            zmsg_addstr(msg_service, txt.c_str());
        }
        int rc = mlm_client_send(self->client, "MESSAGE", &msg_service);
        if (rc != 0)
        {
            log_error("mlm_client_send failed (rc: %d)", rc);
        }
        zmsg_destroy(&msg_service);
    }
}

void static
s_add_msg_services(zmsg_t *msg_service_p, AvahiResolvedServices& avahiServices)
{
    if (!msg_service_p) {
        return;
    }
    for(const AvahiResolvedService avahiService : avahiServices)
    {
        for (const auto& data : std::vector<std::string>({
            std::string("service.hostname=") + avahiService.hostname,
            std::string("service.name=") + avahiService.service.name,
            std::string("service.address=") + avahiService.address,
            std::string("service.port=") + std::to_string(avahiService.port)
        })) {
            zmsg_addstr(msg_service_p, data.c_str());
        }
        for (const auto &txt : avahiService.txt) {
            zmsg_addstr(msg_service_p, txt.c_str());
        }
        zmsg_addstr(msg_service_p, "OK"); // End service
    }
}

//  --------------------------------------------------------------------------
//  process pipe message
//  return true means continue, false means TERM
bool static
s_handle_pipe(fty_mdns_sd_server_t* self, zmsg_t **message_p)
{
    if (! message_p || ! *message_p) return true;
    zmsg_t *message = *message_p;
    bool res = true;

    char *command = zmsg_popstr (message);
    if (!command) {
        zmsg_destroy (message_p);
        log_warning ("Empty command.");
        return true;
    }
    if (streq(command, "$TERM")) {
        log_info ("Got $TERM");
        zmsg_destroy (message_p);
        zstr_free (&command);
        return false;
    }
    else if (streq (command, "CONNECT")) {
        char *endpoint = zmsg_popstr (message);
        if (!endpoint)
            log_error ("%s:\tMissing endpoint", self->name);
        assert (endpoint);
        int r = mlm_client_connect (self->client, endpoint, 5000, self->name);
        if (r == -1)
            log_error ("%s:\tConnection to endpoint '%s' failed", self->name, endpoint);
        log_debug("fty-mdns-sd-server: CONNECT %s/%s",endpoint,self->name);
        zstr_free (&endpoint);
    }
    else if (streq (command, "CONSUMER")) {
        char *stream = zmsg_popstr (message);
        char *pattern = zmsg_popstr (message);
        if (stream && pattern) {
            log_debug("fty-mdns-sd-server: CONSUMER [%s,%s]", stream, pattern);
            int r = mlm_client_set_consumer (self->client, stream, pattern);
            if (r == -1) {
                log_error ("%s:\tSet consumer to '%s' with pattern '%s' failed", self->name, stream, pattern);
            }
        } else {
            log_error ("%s:\tMissing params in CONSUMER command", self->name);
        }
        zstr_free (&stream);
        zstr_free (&pattern);
    }
    else if (streq(command, "PRODUCER-SCAN")) {
        char *scan_topic = zmsg_popstr(message);
        s_set_scan_topic(self, scan_topic);
        // Set default producer for scan
        if (scan_topic) {
            log_debug("fty-mdns-sd-server: PRODUCER-SCAN [%s]", scan_topic);
            int r = mlm_client_set_producer(self->client, scan_topic);
            if (r == -1) {
                log_error("%s:\tSet producer to '%s' failed", self->name, scan_topic);
            }
        } else {
            log_error("%s:\tMissing params in PRODUCER-SCAN command", self->name);
        }
        zstr_free(&scan_topic);
    }
    else if (streq(command, "PRODUCER-NEW-SCAN")) {
        char *scan_new_topic = zmsg_popstr(message);
        s_set_scan_new_topic(self, scan_new_topic);
        zstr_free(&scan_new_topic);
    }
    else if (streq (command, "SET-DEFAULT-SERVICE")) {
         //set new ones
        char *name  = zmsg_popstr (message);
        char *type  = zmsg_popstr (message);
        char *stype = zmsg_popstr (message);
        char *port  = zmsg_popstr (message);
        log_debug("fty-mdns-sd-server: SET-DEFAULT-SERVICE [%s,%s,%s,%s]",
            name,type,stype,port) ;
        s_set_srv_name(self,name);
        s_set_srv_type(self,type);
        s_set_srv_stype(self,stype);
        s_set_srv_port(self,port);
        zstr_free(&name);
        zstr_free(&type);
        zstr_free(&stype);
        zstr_free(&port);
    }
    else if (streq (command, "SET-DEFAULT-TXT")) {
        char *key   = zmsg_popstr (message);
        char *value = zmsg_popstr (message);
        log_debug("fty-mdns-sd-server: SET-DEFAULT-TXT %s=%s",
            key,value) ;
         s_set_txt_record(self,key,value);
        zstr_free(&key);
        zstr_free(&value);
    }
    else if (streq (command, "DO-DEFAULT-ANNOUNCE")) {
        //free previous value
        zstr_free (&self->fty_info_command);
        //get info from fty-info
        self->fty_info_command = zmsg_popstr (message);
        log_debug("fty-mdns-sd-server: DO-DEFAULT-ANNOUNCE %s",
                self->fty_info_command);
        int rv = -1;
        int tries = 3;
        while (tries-- >= 0) {
            rv=s_poll_fty_info(self);
            if (rv == 0)
                break;
            // Wait 5 seconds before retrying
            if (tries == 0)
                zclock_sleep (5000);
        }
        // sanity check, this should trigger a service abort then restart
        // in the worst case, if we did not succeeded after 3 tries
        assert(rv==0);
        self->service->setServiceDefinition(
            self->srv_name,
            self->srv_type,
            self->srv_stype,
            self->srv_port);
        //set all txt properties
        self->service->setTxtRecords (self->map_txt);
        self->service->announce();
    }
    else if (streq (command, "DO-ANNOUNCE")) {
        self->service->setServiceDefinition(
            self->srv_name,
            self->srv_type,
            self->srv_stype,
            self->srv_port);
        // set all txt properties
        self->service->setTxtRecords(self->map_txt);
        self->service->announce();
    }
    else if (streq(command, "SCAN-PARAMETERS")) {
        char *scan_command = zmsg_popstr(message);
        char *scan_type = zmsg_popstr(message);
        self->scan_auto = streq(zmsg_popstr(message), "true");
        self->scan_std_out = streq(zmsg_popstr(message), "true");
        self->scan_no_publish_bus = streq(zmsg_popstr(message), "true");
        s_set_scan_command(self, scan_command);
        s_set_scan_type(self, scan_type);
        zstr_free(&scan_command);
        zstr_free(&scan_type);
        log_trace("scan_auto=%u", self->scan_auto);
        if (self->scan_auto && self->service) {
            self->service->startBrowseNewServices(self->scan_type);
        }
    }
    else if (streq (command, "DO-SCAN")) {
        if (self->service) {
            auto results = self->service->scanServices(self->scan_type);
            // publish result on bus if not deactivated
            if (!self->scan_no_publish_bus) {
                s_publish_msg_services(self, results, nullptr);
            }
            log_info("DO-SCAN scan_std_out=%u", self->scan_std_out);
            // display result on stdout if activated
            if (self->scan_std_out) {
                s_output_services(results);
            }
            res = false;
        }
    }
    else {
        log_warning ("%s:\tUnkown API command=%s, ignoring", self->name, command);
    }
    zstr_free (&command);
    zmsg_destroy (&message);
    return res;
}
//  --------------------------------------------------------------------------
//  process message from ANNOUNCE stream
void static
s_handle_stream(fty_mdns_sd_server_t* self, zmsg_t **message_p)
{
    zmsg_t *message = *message_p;
    char *cmd = zmsg_popstr (message);

    if (cmd) {
        if (streq (cmd, "INFO")) {
            // this suppose to be an update, service must be created already
            char *srv_name = zmsg_popstr (message);
            char *srv_type  = zmsg_popstr (message);
            char *srv_stype = zmsg_popstr (message);
            char *srv_port  = zmsg_popstr (message);

            log_debug("fty-mdns-sd-server: new ANNOUNCEMENT from %s", srv_name);

            zframe_t *infosframe = zmsg_pop (message);
            zhash_t *infos = zhash_unpack (infosframe);
            if (srv_name && srv_type && srv_stype && srv_port && infos) {
                s_set_srv_name (self, srv_name);
                s_set_srv_type (self, srv_type);
                s_set_srv_stype (self, srv_stype);
                s_set_srv_port (self, srv_port);
                self->service->setTxtRecords (infos);
                self->service->update ();
            } else {
                log_error ("Malformed IPC message received");
            }
            zhash_destroy (&infos);
            zframe_destroy (&infosframe);
            zstr_free(&srv_type);
            zstr_free(&srv_stype);
            zstr_free(&srv_port);
            zstr_free (&srv_name);
        }
        // Scan services
        else if (self->scan_command && streq(cmd, self->scan_command)) {
            char *topic_new_scan = zmsg_popstr(message);
            auto results = self->service->scanServices(self->scan_type);
            // publish result on bus if not deactivated
            if (!self->scan_no_publish_bus) {
                s_publish_msg_services(self, results, topic_new_scan);
            }
            // display result on stdout if activated
            if (self->scan_std_out) {
                s_output_services(results);
            }
            if (topic_new_scan) zstr_free(&topic_new_scan);
        }
        else {
            log_error ("Unknown command %s", cmd);
        }
        zstr_free (&cmd);
    }
    zmsg_destroy (message_p);
}

//  --------------------------------------------------------------------------
//  process message from MAILBOX

void static
s_handle_mailbox(fty_mdns_sd_server_t* self, zmsg_t **message_p)
{
    const char *sender = mlm_client_sender(self->client);
    const char *subject = mlm_client_subject(self->client);
    char *message_type = nullptr;
    char *uuid = nullptr;
    char *command = nullptr;
    zmsg_t *reply = nullptr;

    log_debug("%s:\tMAILBOX-DELIVER %s from %s", self->name, subject, sender);

    // bmsg request fty-mdns-sd GET REQUEST 1234 START-SCAN

    // Get inputs, new reply
    message_type = zmsg_popstr(*message_p);
    uuid = zmsg_popstr(*message_p);
    command = zmsg_popstr(*message_p);
    reply = zmsg_new();

    // Check inputs & reply (subject ignored)
    if (!(message_type && uuid && command && reply)) {
        if (!message_type) log_error("%s:\tExpected message type", self->name);
        else if (!uuid) log_error("%s:\tExpected correlation ID", self->name);
        else if (!command) log_error("%s:\tExpected command", self->name);
        else if (!reply) log_error("%s:\tReply allocation failed", self->name);
        else log_error("%s:\tError", self->name);
    }
    else {

        log_debug("%s:\tMAILBOX-DELIVER %s %s %s %s", self->name, subject, message_type, uuid, command);

        // Message model always enforce reply
        zmsg_addstr(reply, uuid);
        zmsg_addstr(reply, "REPLY");
        zmsg_addstr(reply, command);

        // Check message type (handle REQUEST only)
        if (!streq(message_type, "REQUEST")) {
            log_error("%s:\tMAILBOX-DELIVER Invalid message type (%s)", self->name, message_type);
            zmsg_addstr(reply, "ERROR"); // status
            // TBD: translation
            //zmsg_addstrf(reply, TRANSLATE_ME("Invalid message type (%s)").c_str(), message_type);
            zmsg_addstrf(reply, "Invalid message type (%s)", message_type);
        }
        // Handle REQUEST commands
        else if (self->scan_command && streq(command, self->scan_command)) {
            zmsg_addstr(reply, "OK"); // status
            auto results = self->service->scanServices(self->scan_type);
            s_add_msg_services(reply, results);
            log_trace("scan_std_out=%u", self->scan_std_out);
            // display result on stdout if activated
            if (self->scan_std_out) {
                s_output_services(results);
            }
        }
        else {
            log_error("%s:\tMAILBOX-DELIVER Invalid command (%s)", self->name, command);
            zmsg_addstr(reply, "ERROR"); // status
            // TBD: translation
            //zmsg_addstr(reply, /*TRANSLATE_ME*/("Invalid command").c_str());
            zmsg_addstr(reply, "Invalid command");
        }

        // Send reply
        mlm_client_sendto(self->client, sender, subject, NULL, 1000, &reply);
    }

    // Cleanup
    zmsg_destroy(&reply);
    zstr_free(&command);
    zstr_free(&uuid);
    zstr_free(&message_type);
    zmsg_destroy(message_p);
}

//  --------------------------------------------------------------------------
//  Create a new fty_mdns_sd_server

void
fty_mdns_sd_server (zsock_t *pipe, void *args)
{
    char*name=(char*) args;
    fty_mdns_sd_server_t *self = fty_mdns_sd_server_new(name);
    assert (self);

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (self->client), NULL);
    assert (poller);

    // do not forget to send a signal to actor :)
    zsock_signal (pipe, 0);

    log_info ("fty-mdns-sd-server: Started with name '%s'",self->name);

    self->service->start();

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, 100);  // TBD: work with 100ms, don't work with 1000ms ???

        if (which == pipe) {
            zmsg_t *message = zmsg_recv (pipe);
            if(! s_handle_pipe (self, &message)) {
                // send command to exit main
                zstr_sendx(pipe, "STOP", NULL);
                break; // TERM
            }
            // end of command pipe processing
        }
        else if (which == mlm_client_msgpipe (self->client)) {
            zmsg_t *message = mlm_client_recv (self->client);
            const char *command = mlm_client_command (self->client);
            if (streq (command, "STREAM DELIVER")) {
                s_handle_stream (self, &message);
            }
            else
            if (streq (command, "MAILBOX DELIVER")) {
                s_handle_mailbox (self, &message);
            }
        }

        if (self->scan_auto) {
            self->service->waitEvents();
            publish_msg_new_services(self);
        }
    }

    self->service->stop();
    fty_mdns_sd_server_destroy (&self);
    zpoller_destroy (&poller);
}

//  --------------------------------------------------------------------------
//  Self test of this class

//#define AVAHI_DAEMON_RUNNING
#undef AVAHI_DAEMON_RUNNING

void
fty_mdns_sd_server_test (bool verbose)
{
    printf (" * fty_mdns_sd_server: ");

    // @selftest

    // Default service announcement (this test needs avahi-deamon running)
    {
        zactor_t *server = zactor_new(fty_mdns_sd_server, (void*)"fty-mdns-sd-test");
        assert(server);

        zstr_sendx(server, "CONNECT", "ipc://@/malamute", NULL);

        zclock_sleep(1000);

        // do first announcement
#ifdef AVAHI_DAEMON_RUNNING
        zstr_sendx(server, "DO-DEFAULT-ANNOUNCE", "INFO", NULL);
#endif
        zactor_destroy(&server);
    }

    // Specific service announcement and detect it with scan (this test needs avahi-deamon running)
    {
        zactor_t *server = zactor_new(fty_mdns_sd_server, (void*)"fty-mdns-sd-test");
        assert(server);

        zstr_sendx(server, "CONNECT", "ipc://@/malamute", NULL);

        zclock_sleep(1000);

        zuuid_t *uuid = zuuid_new();
        std::string strUuid(zuuid_str_canonical(uuid));
        zuuid_destroy(&uuid);

        std::stringstream buffer;
        buffer << "IPM TEST (" << strUuid << ")";
        std::string name = buffer.str();

        zstr_sendx(server, "SET-DEFAULT-SERVICE", name.c_str(), "_https._tcp.", "_powerservice._sub._https._tcp.", "443", NULL);
        zstr_sendx(server, "SET-DEFAULT-TXT", "txtvers", "1.0.0", NULL);

#ifdef AVAHI_DAEMON_RUNNING
        // do announcement
        zstr_sendx(server, "DO-ANNOUNCE", NULL);

        zclock_sleep(1000);

        // redirect stdout for checking scan result
        buffer.str("");
        std::streambuf *backup = std::cout.rdbuf(buffer.rdbuf());

        // scan all services
        const char *scan_topic = DEFAULT_SCAN_ANNOUNCE;
        const char *scan_type = DEFAULT_SCAN_TYPE;
        bool scan_auto = false;
        bool scan_std_out = true;
        bool scan_no_bus_out = false;
        if (!scan_no_bus_out) zstr_sendx(server, "PRODUCER", scan_topic, NULL);
        zstr_sendx(server, "SCAN-PARAMETERS", "", scan_type, scan_auto ? "true" : "false", scan_std_out ? "true" : "false", scan_no_bus_out ? "true" : "false", NULL);
        zstr_sendx(server, "DO-SCAN", NULL);
        // wait end of scan (TODO timeout ?)
        while (!zsys_interrupted) {
            zmsg_t *msg = zactor_recv(server);
            char *command = zmsg_popstr(msg);
            if (streq(command, "STOP")) {
                break;
            }
            zmsg_destroy(&msg);
        }

        // restore stdout
        std::cout.rdbuf(backup);
        std::cout << buffer.str() << std::endl;

        // test if announcement test has been detected during scan
        // Note: test pass even if avahi daemon not running (local boucle)
        assert(buffer.str().find(name) != std::string::npos);
#endif

        zactor_destroy(&server);
    }

    //  @end
    printf("OK\n");
}

