/*  =========================================================================
    fty-mdns-sd - This service manages network anouncement(mDNS) and discovery (DNS-SD)

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

#ifndef FTY_MDNS_SD_H_H_INCLUDED
#define FTY_MDNS_SD_H_H_INCLUDED

//  Include the project library file
#include "fty_mdns_sd_library.h"

//  Add your own public definitions here, if you need them
static constexpr const uint WAIT_SERVICE_ANNOUNCEMENT_SEC = 5;
static constexpr const char* DEFAULT_SERVICE_NAME      = "IPM (default)";
static constexpr const char* DEFAULT_SERVICE_TYPE      = "_https._tcp.";
static constexpr const char* DEFAULT_SERVICE_SUB_TYPE  = "_powerservice._sub._https._tcp.";
static constexpr const char* DEFAULT_SERVICE_PORT      = "443";

static constexpr const char* DEFAULT_LOG_CONFIG        = "/etc/fty/ftylog.cfg";
static constexpr const char* DEFAULT_SCAN_COMMAND      = "START-SCAN";
static constexpr const char* DEFAULT_SCAN_TOPIC        = "SCAN-ANNOUNCE";
static constexpr const char* DEFAULT_NEW_SCAN_TOPIC    = "SCAN-NEW-ANNOUNCE";
static constexpr const char* DEFAULT_SCAN_TYPE         = "_https._tcp";
static constexpr const char* DEFAULT_SCAN_SUB_TYPE     = "ups,pdu,ats";
static constexpr const char* DEFAULT_SCAN_MANUFACTURER = "";

#endif
