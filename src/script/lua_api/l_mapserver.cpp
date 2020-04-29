/*
Minetest
Copyright (C) 2020 Pierre-Yves Rollo, <dev AT pyrollo DOT com>

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

#include "l_mapserver.h"
#include "cmake_config.h"
#if USE_MAPSERVER
#include "mapserver.h"
#endif
#include "debug.h"
#include "script/lua_api/l_internal.h"
#include <cstring>

enum MapServerStatus {
	STATUS_RUNNING,
	STATUS_STOPPING,
	STATUS_STOPPED
};

enum MapServerMapStatus {
	MAP_NOTREADY,
	MAP_INITIAL,
	MAP_INCREMENTAL,
	MAP_READY
};

std::mutex ModApiMapserver::eventQueueMutex;
std::deque<MapserverEvent> ModApiMapserver::eventQueue;


#if USE_MAPSERVER

// mapserver_start(worldpath)
// Start mapserver using world in worldpath (absolute path)
int ModApiMapserver::l_mapserver_start(lua_State *L) {
	const char *worldpath = luaL_checkstring(L, 1);

	// Go cant declare const args so path must not be const... creating a copy
	// (cant use c_str() neither btw)
	const size_t len = strlen(worldpath);
	char *path = new char[len + 1];
	strncpy(path, worldpath, len + 1);
	MapserverRun(path);
	delete[] path;
	return 0;
}

// mapserver_stop()
// Stops mapserver
int ModApiMapserver::l_mapserver_stop(lua_State *L) {
	MapserverStop();
	return 0;
}
// mapserver_status()
// Returns mapserver application status
int ModApiMapserver::l_mapserver_status(lua_State *L) {
	std::string status_string;
	int status = MapserverStatus();
	switch (status) {
	case STATUS_RUNNING:
		status_string = "running";
		break;
	case STATUS_STOPPING:
		status_string = "stopping";
		break;
	case STATUS_STOPPED:
		status_string = "stopped";
		break;
	default:
		status_string = "unknown";
	}
	lua_pushstring(L, status_string.c_str());
	return 1;
}

// mapserver_map_status()
// returns two values :
// mapserver map status as string
// mapserver map render progress as a number from 0 (0%) to 1 (100%)
int ModApiMapserver::l_mapserver_map_status(lua_State *L) {
	std::string status_string;
	int status = MapserverMapStatus();
	float progress = MapserverMapProgress();
	switch (status) {
	case MAP_NOTREADY:
		status_string = "notready";
		break;
	case MAP_INITIAL:
		status_string = "initial";
		break;
	case MAP_INCREMENTAL:
		status_string = "incremental";
		break;
	case MAP_READY:
		status_string = "ready";
		break;
	default:
		status_string = "unknown";
	}
	lua_pushstring(L, status_string.c_str());
	lua_pushnumber(L, progress);
	return 2;
}
#else

int ModApiMapserver::l_mapserver_start(lua_State *L) { return 0; }
int ModApiMapserver::l_mapserver_stop(lua_State *L) { return 0; }

int ModApiMapserver::l_mapserver_status(lua_State *L) {
	lua_pushstring(L, "unavailable");
	return 1;
}

int ModApiMapserver::l_mapserver_map_status(lua_State *L) {
	lua_pushstring(L, "unavailable");
	lua_pushnumber(L, 0);
	return 2;
}
#endif // USE_MAPSERVER

void ModApiMapserver::Initialize(lua_State *L, int top)
{
	API_FCT(mapserver_start);
	API_FCT(mapserver_stop);
	API_FCT(mapserver_status);
	API_FCT(mapserver_map_status);

#if USE_MAPSERVER
	MapserverListen(&ModApiMapserver::OnEvent);
#endif
}

void ModApiMapserver::InitializeAsync(lua_State *L, int top)
{
	API_FCT(mapserver_start);
	API_FCT(mapserver_stop);
	API_FCT(mapserver_status);
	API_FCT(mapserver_map_status);
}

void ModApiMapserver::Step(lua_State *L)
{
	int error_handler = PUSH_ERROR_HANDLER(L);
	lua_getglobal(L, "core");
	eventQueueMutex.lock();
	while (!eventQueue.empty()) {
		MapserverEvent event = eventQueue.front();
		eventQueue.pop_front();

		lua_getfield(L, -1, "mapserver_event_handler");
		if (lua_isnil(L, -1)) {
			continue;
		}
		luaL_checktype(L, -1, LUA_TFUNCTION);

		lua_pushlstring(L, event.type.data(), event.type.size());

		PCALL_RESL(L, lua_pcall(L, 1, 0, error_handler));
	}
	eventQueueMutex.unlock();
	lua_pop(L, 2); // Pop core and error handler
}

void ModApiMapserver::OnEvent(int eventtype) {
	MapserverEvent event;
	// Would have been much better to get an event name but wasn't able to do it.
	// when passing C.char* C.CString(xxx) on Go side gives a 32 bits truncated
	// pointer provoking a SEGFAULT when used.
	switch(eventtype) {
		case 1:
			event.type = "app-status-changed";
			break;
		case 2:
			event.type = "map-status-changed";
			break;
		default:
			event.type = "unknown";
	}
	eventQueue.push_back(event);
}
