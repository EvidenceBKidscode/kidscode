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

#include "upnpclient.h"
#include "upnpserver.h"
#include "clientiface.h"
#include <mutex>

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

UpnpClient_Handle client_handle;
std::map<std::string, ServerListSpec> servers_list;
std::mutex servers_list_mutex;

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

		IXML_Element *xserver = (IXML_Element *)ixmlNodeList_item(
			xservers, i);

		if (xserver) {
			ServerListSpec server;

			server["address"]     = address;
			server["port"]        = getIntValue(xserver, "kc:port");
			server["name"]        = getElementValue(xserver, "kc:name");
			server["description"] = getElementValue(xserver, "kc:description");
			server["version"]     = getElementValue(xserver, "kc:version");
			server["proto_min"]   = getIntValue(xserver, "kc:proto_min");
			server["proto_max"]   = getIntValue(xserver, "kc:proto_max");

			servers_list_mutex.lock();
			servers_list[uuid] = server;
			servers_list_mutex.unlock();
		}

		// TODO : MANAGE MAPSERVERS

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

void upnp_discovery_start() {};
void upnp_discovery_stop() {};

std::vector<ServerListSpec> upnp_get_server_list() {
	std::vector<ServerListSpec> result;
	return result;
};

#endif // USE_UPNP
