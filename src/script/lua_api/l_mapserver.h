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

/*
	This file is about integration of mapserver into Kidscode.
	LUA API:
	mapserver_start(worldpath): Start mapserver on given world (full path)
	mapserver_stop(): Stops mapserver
	mapserver_status(): Returns status as a string (running, stopping or stopped)

	core.mapserver_event_handler: if not nil, must be a function which is called
		eachtime a Control Event is triggered from mapserver.
*/

#pragma once

#include "l_base.h"
#include <deque>
#include <string>
#include "threading/thread.h"

struct MapserverEvent {
	std::string type;
};

class ModApiMapserver : public ModApiBase
{
public:
	static int l_mapserver_start(lua_State *L);
	static int l_mapserver_stop(lua_State *L);
	static int l_mapserver_status(lua_State *L);
	static int l_mapserver_map_status(lua_State *L);

	static void Initialize(lua_State *L, int top);
	static void InitializeAsync(lua_State *L, int top);
	static void Step(lua_State *L);

	static void OnEvent(int eventtype);

protected:
	// Mutex to protect event queue
	static std::mutex eventQueueMutex;
	// Event queue
	static std::deque<MapserverEvent> eventQueue;
};
