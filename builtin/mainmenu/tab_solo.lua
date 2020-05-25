--Minetest
--Copyright (C) 2020 EvidenceBKidscode
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

function gamemenu.main_button_handler_solo(this, fields, name, tabdata)

	if fields.cb_server then
		core.settings:set("enable_server", fields.cb_server)
		return true
	end


	if fields.mapserver_start and mapmgr.map_is_map(map) and core.mapserver_status() == "stopped" then
		core.mapserver_start(map.path)
	end

	if fields.mapserver_stop  and core.mapserver_status() == "running" then
		core.mapserver_stop()
	end

	if fields.play or (world_doubleclick or fields.key_enter) and mapmgr.map_is_map(map) then
		gamedata.selected_world = map.coreindex

		if core.settings:get_bool("enable_server") then
			if gamedata.selected_world ~= 0 then
				gamedata.playername = fields.te_playername
				gamedata.password   = fields.te_passwd
				gamedata.port       = fields.te_serverport
				gamedata.address    = ""

				core.settings:set("port", gamedata.port)
				if fields.te_serveraddr then
					core.settings:set("bind_address", fields.te_serveraddr)
				end

				core.start()
			else
				gamedata.errormessage = fgettext("No world created or selected!")
			end
		else
			if gamedata.selected_world ~= 0 then
				gamedata.singleplayer = true
				core.start()
			else
				gamedata.errormessage = fgettext("No world created or selected!")
			end
		end
		return true
	end
end

--------------------------------------------------------------------------------
return {
	name = "solo",
	caption = minetest.colorize("#ff0", "    " .. fgettext("Partie solo")),
	cbf_formspec = function(tabview, name, tabdata)
			local fs
			if gamemenu.chosen_map then
				fs = "hypertext[0.2,0.2;8,1;;<big><b>Partie solo - " ..
						gamemenu.chosen_map.name .. "</b></big>]" ..
						"container[0.5,0]" .. formspecs.startsolo.get() .. "container_end[]" ..
						"container[4.5,0]" .. formspecs.mapserver.get() .. "container_end[]" ..
						"container[9.0,0]" .. formspecs.mapinfo.get() .. "container_end[]"
			else
				fs = "hypertext[0.2,0.2;8,1;;<big><b>Partie solo</b></big>]" ..
						formspecs.mapselect.get()
			end
			return fs
		end,

	cbf_button_handler = function(tabview, fields, tabname, tabdata)
			if gamemenu.chosen_map then
				return formspecs.mapinfo.handle(tabview, fields, tabname, tabdata)
						or formspecs.startsolo.handle(tabview, fields, tabname, tabdata)
			else
				return formspecs.mapselect.handle(tabview, fields, tabname, tabdata)
			end
		end,
}
