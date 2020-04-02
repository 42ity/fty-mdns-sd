/*  =========================================================================
    fty_mdns_sd - to manage network announcement(mDNS) and discovery (DNS-SD)
    Copyright (C) 2016 - 2017 Eaton

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
    fty-mdns-sd - to manage network announcement(mDNS) and discovery (DNS-SD)
@discuss
@end
@author Gerald Guillaume <GeraldGuillaume@eaton.com>
*/

#include "fty_mdns_sd_classes.h"
#include "avahi_wrapper.h"

#define DEFAULT_LOG_CONFIG "/etc/fty/ftylog.cfg"

void
usage(){
    puts ("fty-mdns-sd [options] ...");
    puts ("  -v|--verbose        verbose output");
    puts ("  -h|--help           this information");
    puts ("  -c|--config         path to config file");
    puts ("  -e|--endpoint       malamute endpoint [ipc://@/malamute]");
    puts ("  -d|--daemonscan     activate scan daemon");
    puts ("  -a|--autoscan       activate automatic scan");
    puts ("  -s|--scan           scan devices and quit");
    puts ("  -t|--topic          topic name for scan devices result");
    puts ("  -o|--stdout         display scan devices result on standard output");
    puts ("  -n|--nopublishbus   not publish scan devices result on malamute bus");
}

std::string
s_get (zconfig_t *config, const char* key, std::string &dfl) {
    assert (config);
    char *value = zconfig_get(config, key, dfl.c_str());
    if (!value || streq(value, ""))
        return dfl;
    return std::string(value);
}

std::string
s_get (zconfig_t *config, const char *key, const char *dfl) {
    assert (config);
    char *value = zconfig_get(config, key, dfl);
    if (!value || streq(value, "")) {
        return (!dfl) ? std::string(dfl) : std::string("");
    }
    return std::string(value);
}


int
main (int argc, char *argv [])
{
    std::string config_file;
    std::string log_config;
    zconfig_t *config = NULL;

    bool verbose = false;
    int argn;

    log_info ("fty_mdns_sd - started");

    std::string actor_name("fty-mdns-sd");
    std::string endpoint("ipc://@/malamute");
    std::string fty_info_command("INFO");

    bool scan_only = false;
    bool scan_daemon_active = false;
    bool scan_auto = false;
    bool scan_std_out = false;
    bool scan_no_publish_bus = false;
    std::string scan_command(DEFAULT_SCAN_COMMAND);
    std::string scan_default_topic(DEFAULT_SCAN_TOPIC);
    std::string scan_new_topic(DEFAULT_NEW_SCAN_TOPIC);
    std::string scan_type(DEFAULT_SCAN_TYPE);
    std::string scan_sub_type(DEFAULT_SCAN_SUB_TYPE);
    std::string scan_manufacturer(DEFAULT_SCAN_MANUFACTURER);
    std::string scan_filter_key;
    std::string scan_filter_value;

    ManageFtyLog::setInstanceFtylog(actor_name);

    //parse command line
    for (argn = 1; argn < argc; argn++) {
        char *param = NULL;
        if (argn < argc - 1) param = argv [argn+1];

        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            usage();
            return 0;
        }
        else if (streq (argv [argn], "--verbose") || streq (argv [argn], "-v")) {
            verbose = true;
        }
        else if (streq (argv [argn], "--endpoint") || streq (argv [argn], "-e")) {
            if (param) endpoint = param;
            ++argn;
        }
        else if (streq (argv [argn], "--config") || streq (argv [argn], "-c")) {
            if (param) config_file = param;
            ++argn;
        }
        else if (streq (argv [argn], "--scan") || streq (argv [argn], "-s")) {
            scan_only = true;
        }
        else if (streq (argv [argn], "--topic") || streq (argv [argn], "-t")) {
            if (param) scan_default_topic = param;
            ++argn;
        }
        else if (streq (argv [argn], "--daemonscan") || streq (argv [argn], "-d")) {
            scan_daemon_active = true;
        }
        else if (streq (argv [argn], "--autoscan") || streq (argv [argn], "-a")) {
            scan_auto = true;
        }
        else if (streq (argv [argn], "--stdout") || streq (argv [argn], "-o")) {
            scan_std_out = true;
        }
        else if (streq (argv [argn], "--nopublishbus") || streq (argv [argn], "-n")) {
            scan_no_publish_bus = true;
        }
        else {
            printf ("Unknown option: %s\n", argv [argn]);
            return 1;
        }
    }

    //parse config file
    if (!config_file.empty()) {
        log_debug ("fty_mdns_sd:LOAD: %s", config_file.c_str());
        config = zconfig_load(config_file.c_str());
        if (!config) {
            log_error ("Failed to load config file %s: %m", config_file.c_str());
            exit (EXIT_FAILURE);
        }
        verbose = s_get(config, "server/verbose", "false") == std::string("true");
        endpoint = s_get(config, "malamute/endpoint", endpoint);
        actor_name = s_get(config, "malamute/address", actor_name);
        fty_info_command = s_get(config, "fty-info/command", fty_info_command);
        scan_daemon_active = s_get(config, "scan/daemon_active", "false") == std::string("true");
        scan_auto = s_get(config, "scan/auto", "false") == std::string("true");
        scan_std_out = s_get(config, "scan/std_out", "true") == std::string("true");
        scan_no_publish_bus = s_get(config, "scan/no_bus_out", "false") == std::string("true");
        scan_command = s_get(config, "scan/command", scan_command);
        scan_default_topic = s_get(config, "scan/default_scan_topic", scan_default_topic);
        scan_new_topic = s_get(config, "scan/new_scan_topic", scan_new_topic);
        scan_type = s_get(config, "scan/type", scan_type);
        scan_sub_type = s_get(config, "scan/sub_type", scan_sub_type);
        scan_manufacturer = s_get(config, "scan/manufacturer", scan_manufacturer);
        scan_filter_key = s_get(config, "scan/filter_key", scan_filter_key);
        scan_filter_value = s_get(config, "scan/filter_value", scan_filter_value);
        log_config = s_get(config, "log/config", DEFAULT_LOG_CONFIG);
    }
    else {
        log_config = DEFAULT_LOG_CONFIG;
    }
    ManageFtyLog::getInstanceFtylog()->setConfigFile(log_config);
    if (verbose) {
        ManageFtyLog::getInstanceFtylog()->setVeboseMode();
    }

    zactor_t *server = zactor_new(fty_mdns_sd_server, (void*)actor_name.c_str());
    assert(server);
    zstr_sendx(server, "CONNECT", endpoint.c_str(), NULL);
    if (!scan_only) {
        zstr_sendx(server, "CONSUMER", "ANNOUNCE", ".*", NULL);
        log_info("scan_daemon_active=%u", scan_daemon_active);
        if (scan_daemon_active) {
            zstr_sendx(server, "PRODUCER-SCAN", scan_default_topic.c_str(), NULL);
            if (scan_auto) {
                zstr_sendx(server, "PRODUCER-NEW-SCAN", scan_new_topic.c_str(), NULL);
            }
            zstr_sendx(server, "SCAN-PARAMETERS",
                scan_command.c_str(),
                scan_type.c_str(),
                scan_sub_type.c_str(),
                scan_manufacturer.c_str(),
                scan_filter_key.c_str(),
                scan_filter_value.c_str(),
                scan_auto ? "true" : "false",
                scan_std_out ? "true" : "false",
                scan_no_publish_bus ? "true" : "false", NULL);
        }
        ////do first announcement
        zclock_sleep(5000);
        zstr_sendx(server, "DO-DEFAULT-ANNOUNCE", fty_info_command.c_str(), NULL);
    }
    else {
        if (!scan_no_publish_bus) {
            zstr_sendx(server, "PRODUCER-SCAN", scan_default_topic.c_str(), NULL);
        }
        zstr_sendx(server, "SCAN-PARAMETERS",
            scan_command.c_str(),
            scan_type.c_str(),
            scan_sub_type.c_str(),
            scan_manufacturer.c_str(),
            scan_filter_key.c_str(),
            scan_filter_value.c_str(),
            scan_auto ? "true" : "false",
            scan_std_out ? "true" : "false",
            scan_no_publish_bus ? "true" : "false", NULL);
        zstr_sendx(server, "DO-SCAN", NULL);
    }

    while (!zsys_interrupted) {
        zmsg_t *msg = zactor_recv(server);
        char *command = zmsg_popstr(msg);
        if (streq(command, "STOP")) {
            break;
        }
        zmsg_destroy(&msg);
    }
    zactor_destroy(&server);
    zconfig_destroy(&config);
    log_info("fty_mdns_sd - exited");
    return EXIT_SUCCESS;
}
