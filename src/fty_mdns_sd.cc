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
*/

#include "fty_mdns_sd_classes.h"

#include <chrono>
#include <thread>

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

volatile bool g_exit = false;
std::condition_variable g_cv;
std::mutex g_mutex;

void sigHandler(int)
{
    g_exit = true;
    g_cv.notify_one();
}

void setSignalHandler()
{
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sigHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, nullptr);
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
    using Arguments = std::map<std::string, std::string>;

    try {
        std::string configFile;
        std::string logConfig;
        zconfig_t *config = NULL;

        bool verbose = false;
        int argn;

        log_info ("fty_mdns_sd - started");

        MdnsSdServer::Parameters mdnsSdServerParameters;
        mdnsSdServerParameters.actorName = "fty-mdns-sd";
        mdnsSdServerParameters.endpoint = "ipc://@/malamute";
        mdnsSdServerParameters.ftyInfoCommand = "INFO";
        mdnsSdServerParameters.scanCommand = DEFAULT_SCAN_COMMAND;
        mdnsSdServerParameters.scanDefaultTopic = DEFAULT_SCAN_TOPIC;
        mdnsSdServerParameters.scanNewTopic = DEFAULT_NEW_SCAN_TOPIC;

        MdnsSdManager::Parameters mdnsSdManagerParameters;
        mdnsSdManagerParameters.scanOnly = false;
        mdnsSdManagerParameters.scanDaemonActive = false;
        mdnsSdManagerParameters.scanAuto = false;
        mdnsSdManagerParameters.scanStdOut = false;
        mdnsSdManagerParameters.scanNoPublishBus = false;
        mdnsSdManagerParameters.scanType = DEFAULT_SCAN_TYPE;
        mdnsSdManagerParameters.scanSubType = DEFAULT_SCAN_SUB_TYPE;
        mdnsSdManagerParameters.scanManufacturer = DEFAULT_SCAN_MANUFACTURER;
        mdnsSdManagerParameters.scanFilterKey = "";
        mdnsSdManagerParameters.scanFilterValue = "";

        ManageFtyLog::setInstanceFtylog(mdnsSdServerParameters.actorName);

        // Parse command line
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
                if (param) mdnsSdServerParameters.endpoint = param;
                ++argn;
            }
            else if (streq (argv [argn], "--config") || streq (argv [argn], "-c")) {
                if (param) configFile = param;
                ++argn;
            }
            else if (streq (argv [argn], "--scan") || streq (argv [argn], "-s")) {
                mdnsSdManagerParameters.scanOnly = true;
            }
            else if (streq (argv [argn], "--topic") || streq (argv [argn], "-t")) {
                if (param) mdnsSdServerParameters.scanDefaultTopic = param;
                ++argn;
            }
            else if (streq (argv [argn], "--daemonscan") || streq (argv [argn], "-d")) {
                mdnsSdManagerParameters.scanDaemonActive = true;
            }
            else if (streq (argv [argn], "--autoscan") || streq (argv [argn], "-a")) {
                mdnsSdManagerParameters.scanAuto = true;
            }
            else if (streq (argv [argn], "--stdout") || streq (argv [argn], "-o")) {
                mdnsSdManagerParameters.scanStdOut = true;
            }
            else if (streq (argv [argn], "--nopublishbus") || streq (argv [argn], "-n")) {
                mdnsSdManagerParameters.scanNoPublishBus = true;
            }
            else {
                printf ("Unknown option: %s\n", argv [argn]);
                return 1;
            }
        }

        //parse config file
        if (!configFile.empty()) {
            log_debug ("fty_mdns_sd:LOAD: %s", configFile.c_str());
            config = zconfig_load(configFile.c_str());
            if (!config) {
                log_error ("Failed to load config file %s: %m", configFile.c_str());
                exit (EXIT_FAILURE);
            }
            verbose = s_get(config, "server/verbose", "false") == std::string("true");
            mdnsSdServerParameters.endpoint = s_get(config, "malamute/endpoint", mdnsSdServerParameters.endpoint);
            mdnsSdServerParameters.actorName = s_get(config, "malamute/address", mdnsSdServerParameters.actorName);
            mdnsSdServerParameters.ftyInfoCommand = s_get(config, "fty-info/command", mdnsSdServerParameters.ftyInfoCommand);
            mdnsSdManagerParameters.scanDaemonActive = s_get(config, "scan/daemon_active", "false") == std::string("true");
            mdnsSdManagerParameters.scanAuto = s_get(config, "scan/auto", "false") == std::string("true");
            mdnsSdManagerParameters.scanStdOut = s_get(config, "scan/std_out", "true") == std::string("true");
            mdnsSdManagerParameters.scanNoPublishBus = s_get(config, "scan/no_bus_out", "false") == std::string("true");
            mdnsSdServerParameters.scanCommand = s_get(config, "scan/command", mdnsSdServerParameters.scanCommand);
            mdnsSdServerParameters.scanDefaultTopic = s_get(config, "scan/default_scan_topic", mdnsSdServerParameters.scanDefaultTopic);
            mdnsSdServerParameters.scanNewTopic = s_get(config, "scan/new_scan_topic", mdnsSdServerParameters.scanNewTopic);
            mdnsSdManagerParameters.scanType = s_get(config, "scan/type", mdnsSdManagerParameters.scanType);
            mdnsSdManagerParameters.scanSubType = s_get(config, "scan/sub_type", mdnsSdManagerParameters.scanSubType);
            mdnsSdManagerParameters.scanManufacturer = s_get(config, "scan/manufacturer", mdnsSdManagerParameters.scanManufacturer);
            mdnsSdManagerParameters.scanFilterKey = s_get(config, "scan/filter_key", mdnsSdManagerParameters.scanFilterKey);
            mdnsSdManagerParameters.scanFilterValue = s_get(config, "scan/filter_value", mdnsSdManagerParameters.scanFilterValue);
            logConfig = s_get(config, "log/config", DEFAULT_LOG_CONFIG);
        }
        else {
            logConfig = DEFAULT_LOG_CONFIG;
        }
        ManageFtyLog::getInstanceFtylog()->setConfigFile(logConfig);
        if (verbose) {
            ManageFtyLog::getInstanceFtylog()->setVeboseMode();
        }

        // Launch manager
        MdnsSdManager mdnsSdManager(mdnsSdManagerParameters);

        // Launch server
        MdnsSdServer mdnsSdServer(mdnsSdServerParameters, mdnsSdManager);

        mdnsSdManager.init(mdnsSdServer);

        // If scan manual, do scan and exit program
        if (mdnsSdManagerParameters.scanOnly) {
           mdnsSdManager.doScan();
        }
        else {
            // Do default service announcement after timeout specified
            mdnsSdManager.doDefaultAnnounce(WAIT_SERVICE_ANNOUNCEMENT_SEC);

            //
            setSignalHandler();

            // If scan auto activated, wait new devices each time timeout occurred
            if (mdnsSdManagerParameters.scanDaemonActive && mdnsSdManagerParameters.scanAuto) {
                while (1) {
                    // Wait timeout or interrupt.
                    std::unique_lock<std::mutex> lock(g_mutex);
                    // if interrupt occurred, end program
                    if (g_cv.wait_for(lock, std::chrono::milliseconds(100), [](){ return g_exit; })) {
                       break;
                    }
                    // else timeout occurred, read new devices
                    else {
                        mdnsSdManager.getService()->waitEvents();
                        mdnsSdServer.publishMsgNewServices();
                    }
                }
            }
            else {
                // Wait until interrupt
                std::unique_lock<std::mutex> lock(g_mutex);
                g_cv.wait(lock, [](){ return g_exit; });
            }
        }
        return EXIT_SUCCESS;
    }
    catch(std::exception & e)
    {
        log_error ("fty_mdns_sd: Error '%s'", e.what());
    }
    catch(...)
    {
        log_error ("fty_mdns_sd: Error unknown");
    }
    return EXIT_FAILURE;
}
