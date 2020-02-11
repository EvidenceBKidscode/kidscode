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

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <string.h>
#include <mutex>
#include <map>

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

#if USE_UPNP

#include "ixml.h"
#include "upnp.h"
#include "upnptools.h"

// Compatibility stuff (v 1.6 to v1.12)
#ifdef UpnpDiscovery_get_Location_cstr
#define UpnpDiscovery_get_DestAddr(x) (&((x)->DestAddr))
#define HandlerConst
#else
#define HandlerConst const
#endif

#define UPNP_KIDSCODE_DEVICE_TYPE "urn:evidenceb:device:Kidscode:1"

// Libupnp can manage only one interface :(
// So we can manage everything without instanciation

// Client side vars
UpnpClient_Handle client_handle;
std::map<std::string, ServerListSpec> servers_list;
std::mutex servers_list_mutex;

// Server side vars
UpnpDevice_Handle device_handle;
std::string my_uuid;

// Debug display
void upnp_debug( const char * format, ... ) {
	#ifdef DEBUG
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	#endif
}

// Display server list for debugging purpose
void upnp_debug_server_list() {
	#ifdef DEBUG
	for (auto srv : servers_list)
		upnp_debug("UPNP:    %s (%s:%d)\n", srv.first.c_str(),
			srv.second["address"].asCString(),
			srv.second["port"].asInt());
	#endif
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

// get basename
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

// Build upnp xml description file.
// Upnp is only used for anouncement so we put every needed information in this
// file for simplification
std::string get_xml_description(Server *server,
	const std::vector<std::string> &clients_names) {
	std::ostringstream os;

	bool strict_checking = g_settings->getBool("strict_protocol_version_checking");

	std::string name = g_settings->get("server_name");

	if (name == "") {
		name = getBaseName(server->getWorldPath());

		#ifdef WIN32
		TCHAR  infoBuf[32768];
		DWORD  bufCharCount = 32767;
		if(GetComputerName( infoBuf, &bufCharCount ) )
			name = name + " sur " + infoBuf;
		#else
		char hostname[HOST_NAME_MAX] = {0};
		if (!gethostname(hostname, HOST_NAME_MAX))
			name = name + " sur " + hostname;
		#endif
	}

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
	os << "<kc:server>";
	os << "<kc:port>" << server->m_bind_addr.getPort() << "</kc:port>";
	os << "<kc:name>" << name << "</kc:name>";
	os << "<kc:description>" << g_settings->get("server_description") << "</kc:description>"; // TODO: Add more info
	os << "<kc:version>" << g_version_string << "</kc:version>";
	os << "<kc:proto_min>" << (strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MIN) << "</kc:proto_min>";
	os << "<kc:proto_max>" << (strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MAX) << "</kc:proto_max>";
	os << "<kc:creative>" << g_settings->getBool("creative_mode") << "</kc:creative>";
	os << "<kc:damage>" << g_settings->getBool("enable_damage") << "</kc:damage>";
	os << "<kc:password>" << g_settings->getBool("disallow_empty_password") << "</kc:password>";
	os << "<kc:pvp>" << g_settings->getBool("enable_pvp") << "</kc:pvp>";
	os << "<kc:clients>" << clients_names.size() << "</kc:clients>";
	os << "<kc:clients_max>" << g_settings->getU16("max_users") << "</kc:clients_max>";
	os << "</kc:server>";
	os << "</root>";

	return os.str();
}

int server_event_handler(Upnp_EventType event_type, HandlerConst void* event, void* cookie)
{
	// Nothing to handle, everything goes through xml description
	return 0;
}

void upnp_server_start(Server *server,
	const std::vector<std::string> &clients_names) {

	my_uuid = upnp_generate_uuid();
	upnp_debug("UPNP: Starting new server, UUDI=%s\n", my_uuid.c_str());

	// Interface could be given to libupnp but leave it choose one (hoping
	// most computer will have only one interface)
	int ret = UpnpInit2(nullptr, 0);
	if (ret != UPNP_E_SUCCESS) {
		warningstream << "Unable to initialize UPNP (error " << ret
			<< "). Server won't announce on local network."
			<< std::endl;
		return;
	}

	std::string xmldesc = get_xml_description(server, clients_names);

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
}

void upnp_server_shutdown()
{
	UpnpUnRegisterRootDevice(device_handle);
	UpnpFinish();
}

const char *getElementValue(IXML_Element *parent, const char *tagname) {
	IXML_NodeList *nodes = ixmlElement_getElementsByTagName(parent, tagname);
	if (ixmlNodeList_length(nodes) != 1)
		return "";

	IXML_Node *node = ixmlNode_getFirstChild(ixmlNodeList_item(nodes, 0));

	if (node && ixmlNode_getNodeType(node) == eTEXT_NODE)
		return ixmlNode_getNodeValue(node);
	else
		return "";
}

bool getBoolValue(IXML_Element *parent, const char *tagname) {
	std::string value(getElementValue(parent, tagname));
	return (value == "true" || value == "1");
}

int getIntValue(IXML_Element *parent, const char *tagname) {
	return atoi(getElementValue(parent, tagname));
}

int byebye_event_handler(const UpnpDiscovery *e)
{
	upnp_debug("UPNP: Byebye event, UUDI=%s\n", UpnpDiscovery_get_DeviceID_cstr(e));

	if (!strlen(UpnpDiscovery_get_DeviceID_cstr(e)))
		return UPNP_E_SUCCESS;

	servers_list_mutex.lock();
	servers_list.erase(UpnpDiscovery_get_DeviceID_cstr(e));
	servers_list_mutex.unlock();

	upnp_debug("UPNP: Server list is now:\n");
	upnp_debug_server_list();

	return UPNP_E_SUCCESS;
}

int discovery_event_handler(const UpnpDiscovery *e)
{
	upnp_debug("UPNP: Discovery event, UUDI=%s, type=%s\n", UpnpDiscovery_get_DeviceID_cstr(e), UpnpDiscovery_get_DeviceType_cstr(e));

	if (strcmp(UpnpDiscovery_get_DeviceType_cstr(e), UPNP_KIDSCODE_DEVICE_TYPE)) {
		upnp_debug("UPNP:   wrong type\n");
		return UPNP_E_SUCCESS;
	}

	std::string uuid = UpnpDiscovery_get_DeviceID_cstr(e);

	const sockaddr_storage *a = UpnpDiscovery_get_DestAddr(e);

	// Only manage ipv4 for now
	if (a->ss_family != AF_INET) {
		upnp_debug("UPNP:   not IPv4\n");
		return UPNP_E_SUCCESS;
	}

	struct sockaddr_in *p = (struct sockaddr_in *) a;
	std::string address = inet_ntoa(p->sin_addr);

	// Read upnp description where all information about server is.
	IXML_Document *xml = NULL;

	int ret = UpnpDownloadXmlDoc(UpnpDiscovery_get_Location_cstr(e) , &xml);
	if (ret != UPNP_E_SUCCESS) {
		upnp_debug("UPNP:   Unable to load description file\n");
		warningstream << "Unable to load description file (error " << ret
			<< "). May not discover some local network servers."
			<< std::endl;
		return ret;
	}

	if (uuid == "") {
		IXML_NodeList *devices = ixmlDocument_getElementsByTagName(xml, "device");
		if (ixmlNodeList_length(devices) > 0)
			uuid = getElementValue((IXML_Element *)ixmlNodeList_item(
				devices, 0), "UDN");
	}

	if (uuid == "") {
		upnp_debug("UPNP:   no UUID\n");
		return UPNP_E_SUCCESS;
	}

	IXML_NodeList *xservers = ixmlDocument_getElementsByTagName(xml, "kc:server");

	for (unsigned int i = 0; i < ixmlNodeList_length(xservers); i++) {

		ServerListSpec server;

		IXML_Element *xserver = (IXML_Element *)ixmlNodeList_item(
			xservers, i);

		server["address"]     = address;
		server["port"]        = getIntValue(xserver, "kc:port");
		server["name"]        = getElementValue(xserver, "kc:name");
		server["description"] = getElementValue(xserver, "kc:description");
		server["version"]     = getElementValue(xserver, "kc:version");
		server["proto_min"]   = getIntValue(xserver, "kc:proto_min");
		server["proto_max"]   = getIntValue(xserver, "kc:proto_max");
		server["creative"]    = getBoolValue(xserver, "kc:creative");
		server["damage"]      = getBoolValue(xserver, "kc:damage");
		server["password"]    = getBoolValue(xserver, "kc:password");
		server["pvp"]         = getBoolValue(xserver, "kc:pvp");
		server["clients"]     = getIntValue(xserver, "kc:clients");
		server["clients_max"] = getIntValue(xserver, "kc:clients_max");

		servers_list_mutex.lock();
		servers_list[uuid] = server;
		servers_list_mutex.unlock();

		upnp_debug("UPNP: Server list is now:\n");
		upnp_debug_server_list();
	}

	return ret;
}


// TODO : GERER LE TYPE 5 (BYE BYE) !
// Comment ? je ne sais pas.. Peut être refaire un discover ? Puit remplacer les résultats seulement sur timeout
int client_event_handler(Upnp_EventType event_type, HandlerConst void* event, void* cookie)
{
	switch (event_type) {
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_SEARCH_RESULT:
		return discovery_event_handler((UpnpDiscovery *)event);
		break;
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		break;
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		return byebye_event_handler((UpnpDiscovery *)event);
		break;
	default:
		warningstream << "Recieved unhandled UPNP event type " << event_type << "." << std::endl;
	}
	return UPNP_E_SUCCESS;
}

void upnp_discovery_start()
{
	servers_list_mutex.lock();
	servers_list.clear();
	servers_list_mutex.unlock();

	int ret = UpnpInit2(nullptr, 0);
	if (ret != UPNP_E_SUCCESS) {
		warningstream << "Unable to initialize UPNP (error " << ret
			<< "). Won't discover local network servers."
			<< std::endl;
		return;
	}

	ret = UpnpRegisterClient(client_event_handler, nullptr, &client_handle);
	if (ret != UPNP_E_SUCCESS) {
		warningstream << "Unable to register UPNP client (error " << ret
			<< "). Won't discover local network servers."
			<< std::endl;
		UpnpFinish();
		return;
	}

	UpnpSearchAsync(client_handle, 5, "urn:evidenceb:device:Kidscode:1", nullptr);
	if (ret != UPNP_E_SUCCESS) {
		warningstream << "Unable to search UPNP client (error " << ret
			<< "). May not discover some local network servers."
			<< std::endl;
	}
}

void upnp_discovery_stop() {
	UpnpFinish();
}

std::vector<ServerListSpec> upnp_get_server_list() {
	std::vector<ServerListSpec> result;
	servers_list_mutex.lock();
	for (auto srv : servers_list)
		result.push_back(srv.second);
	servers_list_mutex.unlock();
	return result;
}

#else // USE_UPNP

void upnp_server_start(Server *server) {};
void upnp_server_shutdown() {};

void upnp_discovery_start() {};
void upnp_discovery_stop() {};

std::vector<ServerListSpec> upnp_get_server_list() {
	std::vector<ServerListSpec> result;
	return result;
};

#endif // USE_UPNP
