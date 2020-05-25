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

local ESC = core.formspec_escape

formspecs = {}

-- Select map Formspec
----------------------
formspecs.mapselect = {}

function formspecs.mapselect.get()
	local fs = ""
	local selected = tonumber(core.settings:get("mainmenu_last_selected_world")) or 0
	local map, index

	if selected > 0 then
		map = menudata.worldlist:get_raw_element(selected)
		index = filterlist.get_current_index(menudata.worldlist, selected) + 1 -- Header
	else
		index = -math.random() -- Force refresh and avoid selection of title line
	end

	local worldlist = menu_render_worldlist()

	fs = fs ..
		"tooltip[10.8,8;3,0.6;" ..
			core.wrap_text("Cliquez ici pour ajouter une carte téléchargée " ..
				"depuis le site de l'IGN (formats ZIP et RAR acceptés)", 80) .. "]" ..
		"button[10.8,8;3,0.6;world_import;" .. fgettext("Importer une carte") .. "]" ..
		"button[0.2,8;3,0.6;refresh;Mettre à jour la liste]" ..
		"tooltip[refresh;Rafraichir la liste des cartes]" ..
		"tablecolumns[color;text;color;text,padding=1;color;text,align=center,padding=1;" ..
			     "color;text,align=center,padding=1]"

	if map and mapmgr.map_is_demand(map) then
		if mapmgr.can_install_map(map) then
			fs = fs .. "button[3.5,8;2.3,0.6;world_install;" .. fgettext("Installer") .. ";#0000ff]"
		end

		if mapmgr.can_ask_map_again(map) then
			fs = fs .. "button[3.5,8;2,0.6;world_reask;" .. fgettext("Redemander") .. ";#0000ff]"
		end

		if mapmgr.can_cancel_map(map) then
			-- TODO
		end
	end

	if map and mapmgr.map_is_map(map) then
		fs = fs .. "button[3.4,8;3,0.6;world_select;" .. fgettext("Choisir cette carte") .. ";#0000ff]"
	end

	if mapmgr.map_is_map(map) then
		fs = fs ..
			"button[6.6,8;3,0.6;world_delete;" .. fgettext("Supprimer la carte") .. "]"
	end

	if core.settings:get_bool("advanced_options") then
		fs = fs ..
			"button[9.5,4.7;2.2,0.6;world_create;" .. fgettext("Nouveau") .. "]"

		if mapmgr.map_is_map(map) then
			fs = fs ..
				"button[7.1,4.7;2.3,0.6;world_configure;" .. fgettext("Configurer") .. "]"
		end
	end

	local wl = "#ff00ff,Carte,#ff00ff,Demande,#ff00ff,Origine,#ff00ff,Etat"
	wl = wl .. "," .. worldlist

	fs = fs ..
		"table[0.2,0.8;13.6,7;sp_worlds;" .. wl .. ";" .. index .. "]"

	return fs
end

local sort_columns = {
	[2] = "name",
	[4] = "demand",
	[6] = "origin",
	[8] = "status",
}

function formspecs.mapselect.handle(tabview, fields, name, tabdata)
	local world_doubleclick = false

	if fields.sp_worlds then
		local event = core.explode_table_event(fields.sp_worlds)
		if event.type == "DCL" and event.row > 1 then
			world_doubleclick = true
		end

		if event.type == "CHG" then
			if event.row > 1 then
				menu_worldmt_legacy(event.row - 1)
				core.settings:set("mainmenu_last_selected_world",
					menudata.worldlist:get_raw_index(event.row - 1))
			else
				local sort = sort_columns[event.column]
				if sort then
					if menudata.worldlist.m_sortmode == sort then
						menudata.worldlist:reverse_sort()
					else
						menudata.worldlist:set_sortmode(sort_columns[event.column])
					end
				end
				core.settings:set("mainmenu_last_selected_world", 0)
			end
			return true
		end
	end

	if menu_handle_key_up_down(fields, "sp_worlds", "mainmenu_last_selected_world") then
		return true
	end

	local selected = tonumber(core.settings:get("mainmenu_last_selected_world")) or 0
	local map, index

	if selected > 0 then
		map = menudata.worldlist:get_raw_element(selected)
		index = filterlist.get_current_index(menudata.worldlist, selected)
	end

	if fields.refresh then
		menudata.worldlist:refresh()
		return true
	end

	if fields.world_create then
		local create_world_dlg = create_create_world_dlg(true)
		create_world_dlg:set_parent(tabview)
		tabview:hide()
		create_world_dlg:show()
		return true
	end

	if fields.world_import then
		local file_browser_dlg = create_file_browser_dlg()
		file_browser_dlg:set_parent(tabview)
		tabview:hide()
		file_browser_dlg:show()
		return true
	end

	if fields.world_install or (world_doubleclick or fields.key_enter) and mapmgr.can_install_map(map) then
		mapmgr.install_map_from_web(tabview, map)
		return true
	end
	if fields.world_delete then
		if mapmgr.map_is_map(map) then
			local delete_world_dlg = create_delete_world_dlg(map.name, map.coreindex)
			delete_world_dlg:set_parent(tabview)
			tabview:hide()
			delete_world_dlg:show()
		end
		return true
	end

	if fields.world_configure then
		if index then
			local configdialog = create_configure_world_dlg(
				menudata.worldlist:get_raw_index(index))
			if configdialog then
				configdialog:set_parent(tabview)
				tabview:hide()
				configdialog:show()
			end
		end
		return true
	end

	if fields.world_select or (world_doubleclick or fields.key_enter) and mapmgr.map_is_map(map) then
		gamemenu.chosen_map = map
		return true
	end
end

-- Launch Mapserver Formspec
----------------------------
formspecs.mapserver = {}

-- FS width = 3.5
function formspecs.mapserver.get()
	local fs =
	"style[carto;border=false;bgimg_hovered=" .. ESC(defaulttexturedir .. "select.png") .. "]" ..
	"image_button[0.5,1.25;2.5,2.5;" .. ESC(defaulttexturedir .. "img_carto.png") .. ";;]" ..
	"image_button[0.25,1;3,3.5;" .. ESC(defaulttexturedir .. "blank.png") .. ";carto;]" ..
	"hypertext[0.25,4;3,1;map2d;<center><b>Cartographie en 2D</b></center>]"
	return fs
end

-- Launch Singleplayer Game Formspec
------------------------------------

formspecs.startsolo = {}

-- FS width = 3.5
function formspecs.startsolo.get()
	local fs =
		"style[play;border=false;bgimg_hovered=" .. ESC(defaulttexturedir .. "select.png") .. "]" ..
		"image_button[0.5,1.25;2.5,2.5;" .. ESC(defaulttexturedir .. "img_solo.png") .. ";;]" ..
		"hypertext[0.25,4;3,1;play3d;<center><b>Jouer en 3D</b></center>]" ..
		"image_button[0.25,1;3,3.5;" .. ESC(defaulttexturedir .. "blank.png") .. ";play;]"
	return fs
end

function formspecs.startsolo.handle(tabview, fields, name, tabdata)
	if fields.play and gamemenu.chosen_map then
		gamedata = {
			singleplayer = true,
			selected_world = gamemenu.chosen_map.coreindex,
		}
		core.start()
		return true;
	end
end

-- Launch Multiplayer Game Formspec
------------------------------------

formspecs.startmulti = {}

-- FS width = 3.5
function formspecs.startmulti.get()
	local fs =
		"style[play;border=false;bgimg_hovered=" .. ESC(defaulttexturedir .. "select.png") .. "]" ..
		"image_button[0.5,1.25;2.5,2.5;" .. ESC(defaulttexturedir .. "img_multi.png") .. ";;]" ..
		"hypertext[0.25,4;3,1;play3d;<center><b>Jouer en 3D</b></center>]" ..
		"image_button[0.25,1;3,3.5;" .. ESC(defaulttexturedir .. "blank.png") .. ";play;]" ..
		"field[0.25,5;3,0.5;te_playername;Nom / Pseudonyme;" ..
		ESC(core.settings:get("name")) .. "]" ..
		"checkbox[0.25,6;cb_advanced;Options avancées;" ..
		(core.settings:get_bool("cb_advanced") and "true" or "false") .. "]"

	if core.settings:get_bool("cb_advanced") then
		fs = fs ..
				"pwdfield[0.25,6.6;3,0.5;te_passwd;Mot de passe (optionnel)]" ..
				"field[0.25,7.5;3,0.5;te_serverport;Port du serveur;" ..
					ESC(core.settings:get("port")) .. "]"
	end
	return fs
end

function formspecs.startmulti.handle(tabview, fields, name, tabdata)
	if fields.cb_advanced then
		core.settings:set_bool("cb_advanced", fields.cb_advanced == "true")
		return true
	end

	if fields.play and gamemenu.chosen_map then
		gamedata = {
			singleplayer = false,
			selected_world = gamemenu.chosen_map.coreindex,
			playername = fields.te_playername,
			password = fields.te_passwd,
			port = fields.te_serverport or core.settings:get("port"),
			address = "",
		}
		core.settings:set("port", gamedata.port)
		if fields.te_serveraddr then -- Not used but kept in case of change
			core.settings:set("bind_address", fields.te_serveraddr)
		end
		core.start()
		return true;
	end
end

-- Map Information Formspec
---------------------------
formspecs.mapinfo = {}

-- FS width = 4
function formspecs.mapinfo.get()
	local fs = "image[0,1;4,4;".. gamemenu.chosen_map.path ..
			"/worldmods/minimap/textures/scan25.jpg]" ..
			"button[0.5,8;3,0.6;back;Choisir une autre carte]"
	return fs
end

function formspecs.mapinfo.handle(tabview, fields, name, tabdata)
	if fields.back then
		-- TODO: TEST MAPPER
		gamemenu.chosen_map = nil;
		return true
	end
end
