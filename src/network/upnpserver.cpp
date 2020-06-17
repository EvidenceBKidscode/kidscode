/*
Kidscode
Copyright (C) 2020 Pierre-Yves Rollo <dev@pyrollo.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "upnpserver.h"

//#include <iostream>
#include <sstream>
//#include <thread>
//#include <chrono>
#include <string.h>

//#include <map>

#include "server.h"
#include "version.h"
#include "settings.h"
#include "serverlist.h"

#ifdef _WIN32
#include <winbase.h>
#include <iphlpapi.h>
#include <winsock.h>
#else // Linux
#include <unistd.h>
#include <limits.h>
#include <ifaddrs.h>
#endif

//#define DEBUG

#if USE_MAPSERVER
#include "mapserver.h"
#define STATUS_RUNNING 0
#endif


#if USE_UPNP

#include "ixml.h"
#include "upnp.h"
#include "upnptools.h"

// Compatibility stuff (v 1.6 to v1.12)
#ifdef UpnpDiscovery_get_Location_cstr
#define HandlerConst
#else
#define HandlerConst const
#endif


// Libupnp can manage only one interface :(
// So we can manage everything without instanciation

UpnpDevice_Handle device_handle;
std::string my_uuid;
bool server_started = false;

std::string worldpath = "";
std::string xml_world = "";
std::string xml_gameserver = "";
std::string xml_mapserver = "";

// get basename of a path
std::string getBaseName(const std::string& s) {
	size_t i = s.rfind('\\', s.length());
	size_t j = s.rfind('/', s.length());

	if (i == std::string::npos)
		i = j;

	if (j != std::string::npos)
		i = (i > j)?i:j;

	if (i == std::string::npos)
		return s;
	else
		return(s.substr(i+1, s.length() - i));
}

// Change map data according to worldpath
void set_worldpath(std::string path) {
	if (worldpath == path)
		return;
	worldpath = path;

	if (worldpath == "") {
		xml_world == "";
		return;
	}

	// If set, use server_name setting
	std::string name = g_settings->get("server_name");

	// Else, construct a name with mapname and hostname
	if (name == "") {
		name = getBaseName(worldpath);

		#ifdef WIN32
		TCHAR  infoBuf[32768];
		DWORD  bufCharCount = 32767;
		if(GetComputerName( infoBuf, &bufCharCount ) )
			name += " sur " + infoBuf;
		#else
		char hostname[HOST_NAME_MAX] = {0};
		if (!gethostname(hostname, HOST_NAME_MAX))
			name = name + " sur " + hostname;
		#endif
	}

	std::string description = g_settings->get("server_description"); // TODO : Add more info if not set ?

	std::ostringstream os;
	os << "<kc:name>" << name << "</kc:name>";
	os << "<kc:description>" << description << "</kc:description>";
	xml_world = os.str();
}

// Change mapserver data, fetching information from go mapserver lib
void set_mapserver() {
#if USE_MAPSERVER
	if (MapserverStatus() != STATUS_RUNNING) {
		xml_mapserver = "";
		if (xml_gameserver == "")
			set_worldpath("");
		return;
	}
	std::string path = std::string(MapserverGetWorldDir());
	if (worldpath == "")
		set_worldpath(path);

	if (worldpath != path) {
		xml_mapserver = ""; // Mapserver running on another map, do not announce
		return;
	}

	int port = MapserverGetPort();

	std::ostringstream os;
	os << "<kc:mapserver>";
	os << 	"<kc:port>" << port << "</kc:port>";
	os << "</kc:mapserver>";
	xml_mapserver = os.str();
#endif
}

// Change gameserver data according given server
void set_gameserver(Server *server) {
	if (server == nullptr) {
		xml_gameserver = "";
		if (xml_mapserver == "")
			set_worldpath("");
		return;
	}

	std::string path = server->getWorldPath();
	set_worldpath(path);
	set_mapserver(); // Check mapserver again in case of world has changed

	bool strict_checking = g_settings->getBool("strict_protocol_version_checking");

	std::ostringstream os;

	os << "<kc:gameserver>";
	os << 	"<kc:port>" << server->m_bind_addr.getPort() << "</kc:port>";
	os << 	"<kc:version>" << g_version_string << "</kc:version>";
	os << 	"<kc:proto_min>" << (strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MIN) << "</kc:proto_min>";
	os << 	"<kc:proto_max>" << (strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MAX) << "</kc:proto_max>";
	os << "</kc:gameserver>";

	xml_gameserver = os.str();
}

// Generate a random UUID
std::string upnp_generate_uuid() {
	char buffer[40];
	unsigned short int data[8];
	srand(time(NULL) + rand());
	for (int i = 0; i < 8; i++)
		data[i] = rand() % 65536;
	data[4] = (data[4] & 0x0fff) | 0x4000; // Version 4 : Random UUID
	data[5] = (data[5] & 0x3fff) | 0x8000; // Variant 1 : ?
	sprintf(buffer, "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
		data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	return std::string(buffer);
}

// Build upnp xml description file.
// Upnp is only used for anouncement so we put every needed information in this
// file for simplification
std::string get_xml_description() {
	std::ostringstream os;

	os << "<?xml version=\"1.0\"?>";
	os << "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" xmlns:kc=\"urn:schemas-kidscode-org:server-1-0\">";
	os << "<specVersion><major>1</major><minor>0</minor></specVersion>";
	os << "<device>";
	os << "  <deviceType>" << UPNP_KIDSCODE_DEVICE_TYPE << "</deviceType>";
	os << "  <friendlyName>Kidscode</friendlyName>";
	os << "  <manufacturer>EvidenceB</manufacturer>";
	os << "  <modelName>Kidscode</modelName>";
	os << "  <UDN>uuid:" << my_uuid << "</UDN>";
	os << "</device>";

	// Kidscode specific part
	if (xml_world != "") {
		os << "<kc:map>";
		os << xml_world;
		os << xml_gameserver;
		os << xml_mapserver;
		os << "</kc:map>";
	}

	os << "</root>";

	return os.str();
}

int server_event_handler(Upnp_EventType event_type, HandlerConst void* event, void* cookie)
{
	// Nothing to handle, everything goes through xml description
	return 0;
}

void upnp_server_start() {
	my_uuid = upnp_generate_uuid();

	// Interface could be given to libupnp but leave it choose one (hoping
	// most computer will have only one interface)
	int ret = UpnpInit2(nullptr, 0);
	if (ret != UPNP_E_SUCCESS && ret != UPNP_E_INIT) {
		warningstream << "Unable to initialize UPNP (error " << ret
			<< "). Server won't announce on local network."
			<< std::endl;
		return;
	}

	std::string xmldesc = get_xml_description();

	ret = UpnpRegisterRootDevice2(UPNPREG_BUF_DESC, xmldesc.c_str(),
		xmldesc.size(), 1, server_event_handler, nullptr, &device_handle);

	if (ret != UPNP_E_SUCCESS) {
		warningstream << "Unable to initialize UPNP root device (error "
			<< ret << "). Server won't announce on local network."
			<< std::endl;
		UpnpFinish();
		return;
	}
	ret = UpnpSendAdvertisement(device_handle, 0);
	if (ret != UPNP_E_SUCCESS) {
		warningstream << "Unable to send UPNP advertissement (error "
			<< ret << "). Server won't announce on local network."
			<< std::endl;
		UpnpFinish();
		return;
	}
	server_started = true;
}

void upnp_server_shutdown()
{
	UpnpUnRegisterRootDevice(device_handle);
	UpnpFinish();
	server_started = false;
}

// Start or stop server according to game and map severs state

void upnp_server_restart() {
	if (server_started)
		upnp_server_shutdown();

	// Must have a map + at least one server to announce
	if (xml_world == "" || (xml_gameserver == "" && xml_mapserver == ""))
		return;

	upnp_server_start();
}

void upnp_gameserver_started(Server *server)
{
	set_gameserver(server);
	upnp_server_restart();
}

void upnp_gameserver_shutdown() {
	set_gameserver(nullptr);
	upnp_server_restart();
}

void upnp_mapserver_check() {
#if USE_MAPSERVER
	std::string old_xml_world = xml_world;
	std::string old_xml_mapserver = xml_mapserver;
	set_mapserver();

	if (old_xml_world != xml_world || old_xml_mapserver != xml_mapserver) {
		upnp_server_restart();
	}
#endif
}

#else // USE_UPNP

void upnp_gameserver_started(Server *server) {};
void upnp_gameserver_shutdown() {};

void upnp_mapserver_check() {};

#endif // USE_UPNP
