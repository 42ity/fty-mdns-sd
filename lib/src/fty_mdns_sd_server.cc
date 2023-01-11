/*  =========================================================================
    fty_mdns_sd_server - 42ity mdns sd server

    Copyright (C) 2014 - 2020 Eaton

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

//  Structure of our class
struct _fty_mdns_sd_server_t {
    char *name;              // actor name
    mlm_client_t *client;    // malamute client
    char *fty_info_command;
    AvahiWrapper *service;   // service mDNS-SD

    //default service announcement definition
    char *srv_name;
    char *srv_type;
    char *srv_stype;
    char *srv_port;
    //TXT attributes
    zhash_t *map_txt;
};

typedef struct _fty_mdns_sd_server_t fty_mdns_sd_server_t;

//free dynamic item
static void s_destroy_txt(void *arg)
{
    if (arg) {
        free(static_cast<char*>(arg));
    }
}

static void s_set_txt_record(fty_mdns_sd_server_t *self, const char *key, const char *value)
{
    if (!(key && value)) return;
    log_debug ("s_set_txt_record(%s,%s)", key, value);
    char *_value = strdup(value);
    int rv = zhash_insert(self->map_txt, key, _value);
    if (rv == -1) {
        zhash_update(self->map_txt, key, _value);
    }
    zhash_freefn(self->map_txt, key, s_destroy_txt);
}

static void s_set_txt_records(fty_mdns_sd_server_t *self, zhash_t * map_txt)
{
    if (!map_txt) return;
    zhash_destroy (&self->map_txt);
    self->map_txt = zhash_dup(map_txt);
}

static void s_set_srv_name(fty_mdns_sd_server_t *self,const char *value)
{
    if (!value) return;
    zstr_free (&self->srv_name);
    self->srv_name = strdup(value);

}

static void s_set_srv_port(fty_mdns_sd_server_t *self,const char *value)
{
    if (!value) return;
    zstr_free (&self->srv_port);
    self->srv_port = strdup(value);
}

static void s_set_srv_type(fty_mdns_sd_server_t *self,const char *value)
{
    if (!value) return;
    zstr_free (&self->srv_type);
    self->srv_type = strdup(value);
}

static void s_set_srv_stype(fty_mdns_sd_server_t *self,const char *value)
{
    if (!value) return;
    zstr_free (&self->srv_stype);
    self->srv_stype = strdup(value);
}

//  --------------------------------------------------------------------------
//  Create a new fty_mdns_sd_server
static fty_mdns_sd_server_t* fty_mdns_sd_server_new (const char* name)
{
    if (!name) {
        log_error ("name is NULL");
        return NULL;
    }

    fty_mdns_sd_server_t *self = (fty_mdns_sd_server_t *) zmalloc (sizeof (fty_mdns_sd_server_t));
    if (!self)
        return NULL;

    //  Initialize class properties here
    self->name    = strdup (name);
    self->client  = mlm_client_new();
    self->service = new AvahiWrapper(); // service mDNS-SD
    self->map_txt = zhash_new();

    //do minimal initialization
    s_set_txt_record(self, "uuid", "00000000-0000-0000-0000-000000000000");

    return self;
}

//  --------------------------------------------------------------------------
//  Destroy the fty_mdns_sd_server

static void fty_mdns_sd_server_destroy (fty_mdns_sd_server_t **self_p)
{
    if (self_p && (*self_p)) {
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
        if (self->service) delete self->service;

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  get info from fty-info agent

static int s_poll_fty_info(fty_mdns_sd_server_t *self)
{
    assert (self);

    zuuid_t *uuid = zuuid_new ();
    const char* uuid_sent = zuuid_str_canonical (uuid);

    zmsg_t *msg = zmsg_new ();
    zmsg_addstr (msg, self->fty_info_command ? self->fty_info_command : "INFO");
    zmsg_addstr (msg, uuid_sent);
    log_debug ("requesting fty-info ..");
    int r = mlm_client_sendto(self->client, "fty-info", "info", NULL, 1000, &msg);
    zmsg_destroy(&msg);
    if (r != 0) {
        log_error("info: client->sendto (address = '%s') failed.", "fty-info");
        zuuid_destroy(&uuid);
        return -2;
    }

    // get response
    {
        zpoller_t* poller = zpoller_new(mlm_client_msgpipe(self->client), NULL);
        msg = (poller && zpoller_wait(poller, 5000)) ? mlm_client_recv(self->client) : NULL;
        zpoller_destroy(&poller);
        if (!msg) {
            log_error ("info: client->recv (timeout = '5') returned NULL");
            zuuid_destroy(&uuid);
            return -3;
        }
    }

    {
        char *uuid_recv = zmsg_popstr (msg);
        if (!uuid_recv || streq(uuid_recv, "ERROR")) {
            log_error ("info: client->recv returns %s", uuid_recv);
            zstr_free(&uuid_recv);
            zmsg_destroy(&msg);
            zuuid_destroy(&uuid);
            return -4;
        }
        if (!streq(uuid_recv, uuid_sent)) {
            log_error ("info: client->recv uuid mismatch (sent: %s, recv: %s)", uuid_sent, uuid_recv);
            zstr_free(&uuid_recv);
            zmsg_destroy(&msg);
            zuuid_destroy(&uuid);
            return -4;
        }
        zstr_free(&uuid_recv);
    }

    zuuid_destroy(&uuid); // useless
    uuid_sent = NULL;

    {
        char *cmd = zmsg_popstr (msg);
        if (!cmd || !streq(cmd, "INFO")) {
            log_error ("info: client->recv returns %s command", cmd);
            zstr_free (&cmd);
            zmsg_destroy(&msg);
            return -4;
        }
        zstr_free (&cmd);
    }

    {
        char *srv_name  = zmsg_popstr (msg);
        char *srv_type  = zmsg_popstr (msg);
        char *srv_stype = zmsg_popstr (msg);
        char *srv_port  = zmsg_popstr (msg);

        s_set_srv_name(self, srv_name);
        s_set_srv_type(self, srv_type);
        s_set_srv_stype(self, srv_stype);
        s_set_srv_port(self, srv_port);

        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
    }

    {
        zframe_t *frame_infos = zmsg_next (msg);
        zhash_t *infos = zhash_unpack(frame_infos);
        s_set_txt_records(self, infos);
        zhash_destroy(&infos);
    }

    zmsg_destroy(&msg);

    return 0;
}

//  --------------------------------------------------------------------------
//  process pipe message
//  return true means continue, false means TERM
static bool s_handle_pipe(fty_mdns_sd_server_t* self, zmsg_t **message_p)
{
    bool continue_ = true;

    if (! message_p || ! *message_p)
        return continue_;

    zmsg_t *message = *message_p;

    char *command = zmsg_popstr (message);

    if (!command) {
        log_warning ("Empty command.");
    }
    else if (streq(command, "$TERM")) {
        log_debug ("Got $TERM");
        continue_ = false;
    }
    else if (streq (command, "CONNECT")) {
        char *endpoint = zmsg_popstr (message);
        log_debug("fty-mdns-sd-server: CONNECT %s/%s", endpoint, self->name);
        if (!endpoint) {
            log_error ("%s:\tMissing endpoint", self->name);
        }
        else {
            int r = mlm_client_connect (self->client, endpoint, 5000, self->name);
            if (r == -1) {
                log_error ("%s:\tConnection to endpoint '%s' failed", self->name, endpoint);
            }
        }
        zstr_free (&endpoint);
    }
    else if (streq (command, "CONSUMER")) {
        char *stream = zmsg_popstr (message);
        char *pattern = zmsg_popstr (message);
        if (stream && pattern) {
            log_debug("fty-mdns-sd-server: CONSUMER [%s,%s]", stream,pattern) ;
            int r = mlm_client_set_consumer (self->client, stream, pattern);
            if (r == -1) {
                log_error ("%s:\tSet consumer to '%s' with pattern '%s' failed", self->name, stream, pattern);
            }
        }
        else {
            log_error ("%s:\tMissing params in CONSUMER command", self->name);
        }
        zstr_free (&stream);
        zstr_free (&pattern);
    }
    else if (streq (command, "SET-DEFAULT-SERVICE")) {
         //set new ones
        char *name  = zmsg_popstr (message);
        char *type  = zmsg_popstr (message);
        char *stype = zmsg_popstr (message);
        char *port  = zmsg_popstr (message);
        log_debug("fty-mdns-sd-server: SET-DEFAULT-SERVICE [%s,%s,%s,%s]", name, type, stype, port) ;

        s_set_srv_name(self, name);
        s_set_srv_type(self, type);
        s_set_srv_stype(self, stype);
        s_set_srv_port(self, port);

        zstr_free(&name);
        zstr_free(&type);
        zstr_free(&stype);
        zstr_free(&port);
    }
    else if (streq (command, "SET-DEFAULT-TXT")) {
        char *key   = zmsg_popstr (message);
        char *value = zmsg_popstr (message);
        log_debug("fty-mdns-sd-server: SET-DEFAULT-TXT %s=%s", key, value) ;
        s_set_txt_record(self, key, value);
        zstr_free(&key);
        zstr_free(&value);
    }
    else if (streq (command, "DO-DEFAULT-ANNOUNCE")) {
        zstr_free (&self->fty_info_command);
        self->fty_info_command = zmsg_popstr (message);
        log_debug("fty-mdns-sd-server: DO-DEFAULT-ANNOUNCE %s", self->fty_info_command);

        int rv = -1;
        int tries = 3;
        while (tries-- >= 0) {
            rv = s_poll_fty_info(self);
            if (rv == 0)
                break;
        }
        // sanity check, this should trigger a service abort then restart
        // in the worst case, if we did not succeeded after 3 tries
        if (rv != 0) {
            log_error("s_poll_fty_info() failed");
        }
        else if (!self->service) {
            log_error("self->service is NULL");
        }
        else {
            self->service->setServiceDefinition(
                self->srv_name  ? self->srv_name  : "",
                self->srv_type  ? self->srv_type  : "",
                self->srv_stype ? self->srv_stype : "",
                self->srv_port  ? self->srv_port  : "");
            // set all txt properties
            self->service->setTxtRecords (self->map_txt);
            self->service->start();
        }
    }
    else {
        log_warning ("%s:\tUnkown API command=%s, ignoring", self->name, command);
    }

    zstr_free (&command);
    zmsg_destroy (message_p);

    return continue_;
}

//  --------------------------------------------------------------------------
//  process message from ANNOUNCE stream
static void s_handle_stream(fty_mdns_sd_server_t* self, zmsg_t **message_p)
{
    if (!(message_p && *message_p))
        return; // secured

    zmsg_t *message = *message_p;
    char *cmd = zmsg_popstr (message);

    if (cmd && streq (cmd, "INFO")) {
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
        }
        else {
            log_error ("Malformed INFO message received");
        }
        zhash_destroy (&infos);
        zframe_destroy (&infosframe);

        zstr_free(&srv_name);
        zstr_free(&srv_type);
        zstr_free(&srv_stype);
        zstr_free(&srv_port);
    }
    else {
        log_error ("Unknown command %s", cmd);
    }

    zstr_free (&cmd);
    zmsg_destroy (message_p);
}

//  --------------------------------------------------------------------------
//  process message from MAILBOX

static void s_handle_mailbox(fty_mdns_sd_server_t* /*self*/, zmsg_t **message_p)
{
    //nothing to do for now
    zmsg_destroy (message_p);
}

//  --------------------------------------------------------------------------
//  Create a new fty_mdns_sd_server

void fty_mdns_sd_server (zsock_t *pipe, void *args)
{
    char* name = static_cast<char*>(args);

    fty_mdns_sd_server_t *self = fty_mdns_sd_server_new(name);
    if (!self) {
        log_error("self alloc failed");
        return;
    }

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (self->client), NULL);
    if (!poller) {
        log_error("poller alloc failed");
        fty_mdns_sd_server_destroy (&self);
        return;
    }

    // do not forget to send a signal to actor :)
    zsock_signal (pipe, 0);

    log_info ("%s started", self->name);

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, -1);

        if (which == pipe) {
            zmsg_t *message = zmsg_recv (pipe);
            bool continue_ = s_handle_pipe (self, &message);
            zmsg_destroy(&message);
            if (!continue_) {
                break; // TERM
            }
        }
        else if (which == mlm_client_msgpipe (self->client)) {
            zmsg_t *message = mlm_client_recv (self->client);
            const char *command = mlm_client_command (self->client);
            if (streq (command, "STREAM DELIVER")) {
                s_handle_stream (self, &message);
            }
            else if (streq (command, "MAILBOX DELIVER")) {
                s_handle_mailbox (self, &message);
            }
            zmsg_destroy(&message);
        }
    }

    log_info ("%s ended", self->name);

    self->service->stop();
    zpoller_destroy (&poller);
    fty_mdns_sd_server_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void fty_mdns_sd_server_test (bool /*verbose*/)
{
    printf (" * fty_mdns_sd_server: \n");

    //  Simple create/destroy test

    zactor_t *server = zactor_new (fty_mdns_sd_server, const_cast<char*>("fty-mdns-sd-test"));
    assert (server);

    zstr_sendx (server, "CONNECT", "ipc://@/malamute", NULL);

    zclock_sleep (1000);

    zstr_sendx (server, "SET-DEFAULT-SERVICE",
            "IPC (12345678)","_https._tcp.","_powerservice._sub._https._tcp.","443", NULL);
    zstr_sendx (server, "SET-DEFAULT-TXT",
            "txtvers","1.0.0",NULL);

    //do first announcement
    //this test needs avahi-deamon running
    //zstr_sendx (server, "DO-DEFAULT-ANNOUNCE", "INFO",NULL);

    zactor_destroy (&server);

    printf (" * fty_mdns_sd_server: OK\n");
}
