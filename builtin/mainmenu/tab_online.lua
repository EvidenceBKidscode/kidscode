--Minetest
--Copyright (C) 2014 sapier
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

local ESC = core.formspec_escape

--------------------------------------------------------------------------------
local function get_formspec(tabview, name, tabdata)
	-- Update the cached supported proto info,
	-- it may have changed after a change by the settings menu.
	common_update_cached_supp_proto()

	tabdata.servers = core.get_favorites("lan")

	local selected_server
	selected_server = tabdata.servers[tabdata.selected_server]

	local fs =
		"hypertext[0.2,0.2;8,1;;<big><b>Rejoindre une partie multijoueur</b></big>]" ..
		"tableoptions[background=#00000000;border=false]"

	if selected_server then
		if selected_server.gameserver then
			fs = fs ..
				"style[btn_mp_play;border=false;bgimg_hovered=" ..
					ESC(defaulttexturedir .. "select.png") .. "]" ..
				"image_button[0.6,6.2;2.5,2.5;" ..
					ESC(defaulttexturedir .. "img_multi.png") .. ";;]" ..
				"image_button[0.4,6;2.9,3.3;" ..
					ESC(defaulttexturedir .. "blank.png") .. ";btn_mp_play;]" ..
				"label[0.8,9;Rejoindre la partie]" ..
				"label[8,8.9;" .. fgettext("Nom / Pseudonyme :") .. "]" ..
				"field[10.6,8.65;3.2,0.5;te_name;;" .. ESC(core.settings:get("name")) .. "]"
		end

		if selected_server.mapserver then
			fs = fs ..
				"style[btn_mp_carto;border=false;bgimg_hovered=" ..
					ESC(defaulttexturedir .. "select.png") .. "]" ..
				"image_button[4,6.2;2.5,2.5;" ..
					ESC(defaulttexturedir .. "img_carto.png") .. ";;]" ..
				"image_button[3.8,6;2.9,3.3;" ..
					ESC(defaulttexturedir .. "blank.png") .. ";btn_mp_carto;]" ..
				"label[4.15,9;Cartographie en 2D]"

		end
	end

	--servers
	if #tabdata.servers == 0 then
		fs = fs ..
			[[hypertext[0.2,2.8;13.6,3;noserver;<center>
				<big><b>Aucune partie disponible</b></big>
				<b>Attendez qu'une partie commence pour la rejoindre</b>]
			]]
	else
		fs = fs .. "label[0.2,1.2;Parties disponibles :]"
		fs = fs .. "tablecolumns[text]"
		fs = fs .. "table[0.2,1.5;13.6,4.3;servers;"

		for i = 1, #tabdata.servers do
			if i > 1 then
				fs = fs .. ","
			end
			fs = fs .. render_serverlist_row(tabdata.servers[i])
		end

		if tabdata.selected_server then
			fs = fs .. ";" .. tabdata.selected_server .. "]"
		else
			fs = fs .. ";0]"
			core.settings:set("address", "")
			core.settings:set("remote_port", "30000")
		end
	end

	return fs
end

--------------------------------------------------------------------------------
local function start_gameserver(server)
	if not is_server_protocol_compat_or_error(
			server.proto_min, server.proto_max) then
		return
	end

	core.volatile_settings:set("mainmenu_last_tab", "online")

	gamedata = {
		playername     = core.settings:get("name"),
		address        = server.address,
		port           = server.port,
		selected_world = 0,
	}
	core.start()
end

--------------------------------------------------------------------------------
local function main_button_handler(tabview, fields, name, tabdata)
	local serverlist = tabdata.servers

	if fields.te_name then
		core.settings:set("name", fields.te_name)
	end

	if fields.servers then
		local event = core.explode_table_event(fields.servers)
		local server = serverlist[event.row]

		if event.type == "DCL" then
			if event.row <= #serverlist then
				if not is_server_protocol_compat_or_error(
						server.proto_min, server.proto_max) then
					return true
				end
				if server.gameserver then
					start_gameserver(server)
				end
			end
			return true
		end

		if event.type == "CHG" then
			if event.row <= #serverlist then
				tabdata.selected_server = event.row
			end
			return true
		end
	end

	if fields.key_up or fields.key_down then
		local idx = core.get_table_index("servers")

		if idx then
			if fields.key_up and idx > 1 then
				idx = idx - 1
			elseif fields.key_down and idx < #serverlist then
				idx = idx + 1
			end
		else
			idx = 1
		end

		if not serverlist or not serverlist[idx] then
			tabdata.server_selected = 0
			return true
		end

		tabdata.server_selected = idx
		return true
	end

	-- Actions on selected server
	-----------------------------

	local server = tabdata.servers[tabdata.selected_server]
	if not server then
		return
	end

	-- Join game
	if (fields.btn_mp_play or fields.key_enter) and server.gameserver then
		start_gameserver(server)
		return true
	end

	-- Launch browser on distant map server.
	if fields.btn_mp_carto and server.mapserver then
		core.launch_browser( "http://" .. server.mapserveraddress .. ":" ..
				server.mapserverport)
		return true
	end

	return false
end

--------------------------------------------------------------------------------
-- Async stuff

function core.handle_async(func, parameter, callback)
	-- Serialize function
	local serialized_func = string.dump(func)

	assert(serialized_func ~= nil)

	-- Serialize parameters
	local serialized_param = core.serialize(parameter)

	if serialized_param == nil then
		return false
	end

	local jobid = core.do_async_callback(serialized_func, serialized_param)

	core.async_jobs[jobid] = callback

	return true
end

local autosync

local function wait()
	local time = os.time()
	while (os.time() == time) do
	end
end

local function do_autosync()
	if autosync then
		core.event_handler("Refresh")
		core.handle_async(wait, nil, do_autosync)
	end
end

local function on_change(type, old_tab, new_tab)
	if type == "LEAVE" then
		autosync = false
	end

	if type == "ENTER" then
		autosync = true
		do_autosync()
	end
end

--------------------------------------------------------------------------------
return {
	name = "online",
	caption = minetest.colorize("#ff0", fgettext("Rejoindre partie multijoueur")),
	cbf_formspec = get_formspec,
	cbf_button_handler = main_button_handler,
	on_change = on_change,
}
