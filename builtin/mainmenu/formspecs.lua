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
local min, max = math.min, math.max

formspecs = {}

-- Sort mechanisms
------------------

local function map_sort(self, a, b, aa, bb)
	if a == nil or b == nil then
		return b == nil and a ~= nil
	end
	if aa == bb then
		if self.m_sort_forward then
			return stripAccents(a.name):upper() < stripAccents(b.name):upper()
		else
			return stripAccents(a.name):upper() > stripAccents(b.name):upper()
		end
	else
		if self.m_sort_forward then
			return aa < bb
		else
			return aa > bb
		end
	end
end

--------------------------------------------------------------------------------
local status_order = { installed = 1, ready = 2, prepare = 3 }

function formspecs.sort_worlds_by_status(self)
	table.sort(self.m_processed_list, function(a, b)
		return map_sort(self, a, b,
			a and status_order[a.status] or 100,
			b and status_order[b.status] or 100)
	end)
end

--------------------------------------------------------------------------------

function formspecs.sort_worlds_by_demand(self)
	table.sort(self.m_processed_list, function(a, b)
		return map_sort(self, a, b,
			a and a.alac and a.alac.requested_on or "Z",
			b and b.alac and b.alac.requested_on or "Z")
	end)
end

--------------------------------------------------------------------------------

function formspecs.sort_worlds_by_origin(self)
	table.sort(self.m_processed_list, function(a, b)
		return map_sort(self, a, b, a and a.origin, b and b.origin)
	end)
end

--------------------------------------------------------------------------------

function formspecs.sort_worlds_by_filesize(self)
	table.sort(self.m_processed_list, function(a, b)
		return map_sort(self, a, b,
			a and (a.filesize or 0),
			b and (b.filesize or 0))
	end)
end

--------------------------------------------------------------------------------

function formspecs.sort_worlds_by_mapsize(self)
	table.sort(self.m_processed_list, function(a, b)
		return map_sort(self, a, b,
			a and (a.mapsize or 0),
			b and (b.mapsize or 0))
	end)
end

-- Select map Formspec
----------------------
formspecs.mapselect = {}

local table_columns = {
	{ "name",     "Carte",   "text,width=15"},
	{ "status",   "Etat",    "text,align=center,width=6"},
	{ "demand",   "Date",    "text,width=6"},
	{ "mapsize",  "Emprise", "text,align=center,width=7"},
	{ "filesize", "Taille",  "text,align=center,width=6"},
	{ "origin",   "Origine", "text,align=center,width=6"},
}

function formspecs.mapselect.get()
	local fs = ""
	local uid = core.settings:get("mainmenu_last_selected_world_uid")
	local map
	local index = 1

	if uid then
		map = menudata.worldlist:get_raw_element(menudata.worldlist:raw_index_by_uid(uid))
		index = filterlist.get_current_index_by_uid(menudata.worldlist, uid) + 1 -- Header
	end
	if index == 1 then
		index = -1
	end

	fs = fs ..
		"button[9.8,7.3;4,0.6;world_import;" .. fgettext("Importer une carte") .. "]" ..
		"tooltip[world_import;" ..
			core.wrap_text("Cliquez ici pour ajouter une carte téléchargée " ..
				"depuis le site de l'IGN (formats ZIP et RAR acceptés)", 80) .. "]" ..
		"button[9.8,8;4,0.6;refresh;Actualiser la liste]" ..
		"tooltip[refresh;Rafraichir la liste des cartes]"

	if map then
		if mapmgr.map_is_map(map) then
			fs = fs ..
				"style[world_select;bgcolor=#0000ff]" ..
				"style[world_delete;bgcolor=#ff0000]" ..
				"button[0.2,8;4,0.6;world_select;" .. fgettext("Choisir cette carte") .. "]" ..
				"button[0.2,8.7;4,0.6;world_delete;" .. fgettext("Désinstaller la carte") .. "]" ..
				"button[0.2,7.3;4,0.6;world_rename;" .. fgettext("Renommer la carte") .. "]"
		end

		if mapmgr.map_is_demand(map) then
			if mapmgr.can_install_map(map) then
				fs = fs ..
					"style[world_install;bgcolor=#0000ff]" ..
					"button[0.2,8;4,0.6;world_install;" .. fgettext("Installer") .. "]"
			end

			if mapmgr.can_ask_map_again(map) then
				fs = fs ..
					"style[world_reask;bgcolor=#0000ff]" ..
					"button[0.2,8;4,0.6;world_reask;" .. fgettext("Redemander") .. "]"
			end
		end

		if mapmgr.can_cancel_map(map) then
			-- TODO
		end
	end

	if core.settings:get_bool("advanced_options") then
		fs = fs ..
			"button[9.8,8;4,0.6;world_create;" .. fgettext("Nouveau") .. "]"

		if map and mapmgr.map_is_map(map) then
			fs = fs ..
				"button[9.8,8.7;4,0.6;world_configure;" .. fgettext("Configurer") .. "]"
		end
	end

	local row1 = ""
	local options
	for _, column in ipairs(table_columns) do
		local label = column[2]
		if menudata.worldlist.m_sortmode == column[1] then
			if menudata.worldlist.m_sort_forward then
				label = label .. " ▼"
			else
				label = label .. " ▲"
			end
		end
		row1 = row1 .. "#aaa," .. label .. ","
		if options then
			options = options .. ";color;" .. column[3]
		else
			options = "color;" .. column[3]
		end
	end

	fs = fs ..
		"tableoptions[background=#00000025;highlight=#45B;border=false]" ..
		"tablecolumns[" .. options .. "]"
	if mapmgr.errors.network then
		fs = fs ..
			"table[0.2,0.8;13.6,5.4;sp_worlds;" .. row1 .. menu_render_worldlist() .. ";" .. index .. "]" ..
			"label[0.2,6.5;" ..	minetest.colorize("#F42",
				"La liste des demandes de cartes n'a pu être téléchargée, verifier vos paramètres réseau et proxy.") ..
			"]"
	else
		fs = fs ..
			"table[0.2,0.8;13.6,5.9;sp_worlds;" .. row1 .. menu_render_worldlist() .. ";" .. index .. "]"
	end

	fs = fs ..
		"label[0.2,7;Sélectionnez une carte dans la liste ou importez une carte " ..
			"au format .zip ou .rar via le bouton \"importer\".]" ..
		"label[-10,-10;" .. math.random() .. "]" -- Force refresh
		-- Refresh is forced by adding a hidden random element making form
		-- always different. Without forced refresh it could happen to have
		-- the wrong list line selected (header in particular)
	return fs
end

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
				local map = menudata.worldlist:get_raw_element(
					menudata.worldlist:get_raw_index(event.row - 1))
				if map then
					core.settings:set("mainmenu_last_selected_world_uid", map.uid)
				end
			else
				local sort = table_columns[event.column / 2]
				if sort then
					sort = sort[1]
					if menudata.worldlist.m_sortmode == sort then
						menudata.worldlist:reverse_sort()
					else
						menudata.worldlist:set_sortmode(sort)
					end
				end
			end

			return true
		end
	end

	-- Manages key up/down if focus not in list
	if fields.key_up or fields.key_down then
		local oldidx, newidx = core.get_textlist_index("sp_worlds") - 1, 1
		if fields.key_up and oldidx and oldidx > 1 then
			newidx = oldidx - 1
		elseif fields.key_down and oldidx and
				oldidx < menudata.worldlist:size() then
			newidx = oldidx + 1
		end
		local map = menudata.worldlist:get_raw_element(
			menudata.worldlist:get_raw_index(newidx))
		if map then
			core.settings:set("mainmenu_last_selected_world_uid", map.uid)
		end
		return true
	end

	local uid = core.settings:get("mainmenu_last_selected_world_uid")
	local map, index

	if uid then
		map = menudata.worldlist:get_raw_element(
			menudata.worldlist:raw_index_by_uid(uid))
		index = filterlist.get_current_index_by_uid(menudata.worldlist, uid) + 1 -- Header
	else
		index = -math.random() -- Force refresh and avoid selection of title line
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
			local configdialog = create_configure_world_dlg(menudata.worldlist:get_raw_index(index))
			if configdialog then
				configdialog:set_parent(tabview)
				tabview:hide()
				configdialog:show()
			end
		end
		return true
	end

	if fields.world_rename then
		mapmgr.rename_map(tabview, map)
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
		"style[mapserver_start;border=false;bgimg_hovered=" .. ESC(defaulttexturedir .. "select.png") .. "]" ..
		"image_button[0.5,1.25;2.5,2.5;" .. ESC(defaulttexturedir .. "img_carto.png") .. ";;]" ..
		"image_button[0.25,1;3,3.5;" .. ESC(defaulttexturedir .. "blank.png") .. ";mapserver_start;]" ..
		"hypertext[0.25,4;3,1;map2d;<center><b>Cartographier en 2D</b></center>]"

	local serverstatus = core.mapserver_status()
	local mapstatus, mapprogress = core.mapserver_map_status()
	mapprogress = mapprogress or 0

	if serverstatus == "running" then
		fs = fs .. "button[0,7.8;4,0.6;mapserver_stop;Arrêter le cartographe]"

		local status = ""

		if mapstatus == "notready" then
			status = "Carte en préparation"
		elseif mapstatus == "initial" then
			status = ("Rendu initial de la carte (%d%%)"):format(mapprogress * 100)
			fs = fs .. "box[0.25,6;3,0.2;#CCCCFFFF]"
			fs = fs .. "box[0.25,6;" .. (3 * mapprogress) .. ",0.2;#8888FFFF]"
		elseif mapstatus == "incremental" then
			status = ("Mise à jour de la carte (%d%%)"):format(mapprogress * 100)
			fs = fs .. "box[0.25,6;3,0.2;#CCCCFFFF]"
			fs = fs .. "box[0.25,6;" .. (3 * mapprogress) .. ",0.2;#8888FFFF]"
		elseif mapstatus == "ready" then
			status = "Carte prête"
		end

		fs = fs ..
			"hypertext[0.25,5;3,1.5;mapserver_status;<center>Cartographe en fonction\n" ..
				status .. "</center>]"
	end

	if serverstatus == "stopping" then
		fs = fs .. "hypertext[0.25,5;3,1.5;mapserver_status;<center>Cartographe en cours d'arrêt</center>]"
	end


	return fs
end

function formspecs.mapserver.handle(tabview, fields, name, tabdata)
	if fields.mapserver_start and gamemenu.chosen_map then
		if core.mapserver_status() == "stopped" then
			core.mapserver_start(gamemenu.chosen_map.path)
		end
		core.launch_browser("http://localhost:8080")
	end

	if fields.mapserver_stop  and core.mapserver_status() == "running" then
		core.mapserver_stop()
	end
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

		core.volatile_settings:set("mainmenu_last_tab", "solo")
		core.volatile_settings:set("mainmenu_last_chosen_world_uid",
			core.settings:get("mainmenu_last_selected_world_uid"))

		gamedata = {
			singleplayer = true,
			selected_world = gamemenu.chosen_map.coreindex,
		}
		core.start()
		return true
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
		"container[0,0.5]" ..
		"field[0.25,5;3,0.5;te_playername;Nom / Pseudonyme;" ..
			ESC(core.settings:get("name")) .. "]" ..
		"checkbox[0.25,6;cb_advanced;Options avancées;" ..
			(core.settings:get_bool("cb_advanced") and "true" or "false") .. "]"

	if core.settings:get_bool("cb_advanced") then
		fs = fs .. "pwdfield[0.25,6.6;3,0.5;te_passwd;Mot de passe (optionnel)]" ..
			   "field[0.25,7.5;3,0.5;te_serverport;Port du serveur;" ..
				ESC(core.settings:get("port")) .. "]"
	end

	fs = fs .. "container_end[]"

	return fs
end

function formspecs.startmulti.handle(tabview, fields, name, tabdata)
	if fields.cb_advanced then
		core.settings:set_bool("cb_advanced", fields.cb_advanced == "true")
		return true
	end

	if fields.play and gamemenu.chosen_map then
		core.volatile_settings:set("mainmenu_last_tab", "multi")
		core.volatile_settings:set("mainmenu_last_chosen_world_uid",
			core.settings:get("mainmenu_last_selected_world_uid"))

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
		return true
	end
end

-- Map Information Formspec
---------------------------
formspecs.mapinfo = {}

function stop_mapserver_confirm(parent, cb)
	local dlg =
		dialog_create("dlg_confirm", function()
			return  "size[8,3]" ..
				"label[0,0.5;Cette action requiert l'arrêt du cartographe. Confirmer ?]" ..
				"button[1,2;2.6,0.5;dlg_mapimport_formspec_ok;OK]" ..
				"button[4,2;2.8,0.5;dlg_mapimport_formspec_cancel;Annuler]"
		end,
		function(this, fields, data)
			this:delete()
			ui.update()
			if fields.dlg_mapimport_formspec_ok then
				core.mapserver_stop()
				cb()
			end
		end)

	dlg:set_parent(parent)
	parent:hide()
	dlg:show()
	ui.update()

	return true
end

local function get_minmax_xy(coords)
	local xmin, xmax, ymin, ymax
	for _, point in ipairs(coords) do
		xmin = xmin and min(xmin, point[1]) or point[1]
		xmax = xmax and max(xmax, point[1]) or point[1]
		ymin = ymin and min(ymin, point[2]) or point[2]
		ymax = ymax and max(ymax, point[2]) or point[2]
	end
	return  xmin, xmax, ymin, ymax
end

local function degrees_to_dms(degrees)
	local d = math.floor(degrees)
	local m = math.floor((degrees - d) * 60)
	local s = ((degrees - d) * 60 - m) * 60

	return string.format("%d° %d' %.1f\"", d, m, s)
end

-- FS width = 4
function formspecs.mapinfo.get()
	local fs = "button[-4.5,8.5;4,0.6;back;Choisir une autre carte]"

	local mapimage = ESC(gamemenu.chosen_map.path .. DIR_DELIM ..
		"worldmods" .. DIR_DELIM .. "minimap" .. DIR_DELIM ..
		"textures" .. DIR_DELIM .. "scan25.jpg")

	if file_exists(mapimage) then
		fs = fs .. "image[0,1;4,4;" .. mapimage .. "]"
	else
		fs = fs .. "box[0,1;4,4;#222]" ..
			"hypertext[0,2.9;4,2;;<b><center>Pas d'aperçu disponible</center></b>]"
	end

	local text = ""
	if mapmgr.get_geometry(gamemenu.chosen_map) then
		local lat, lon, cpt = 0, 0, 0
		for _, point in ipairs(gamemenu.chosen_map.geo.coordinatesGeo) do
			lat = lat + point[1]
			lon = lon + point[2]
			cpt = cpt + 1
		end

		text = text ..
			"Longitude <b>" .. degrees_to_dms(math.abs(lon / cpt)) ..
				(lon > 0 and "E" or "O") .. "</b>\n" ..
			"Latitude <b>" .. degrees_to_dms(math.abs(lat / cpt)) ..
				(lat > 0 and "N" or "S") .. "</b>\n"

		local xmin, xmax, ymin, ymax = get_minmax_xy(gamemenu.chosen_map.geo.coordinatesCarto)
		local _xmin, _xmax, _ymin, _ymax = get_minmax_xy(gamemenu.chosen_map.geo.coordinatesGame)
		local kmx, kmy = (xmax - xmin) / 1000, (ymax - ymin) / 1000

		text = text ..
			("Taille de la carte : <b>%0.1f</b> x <b>%0.1f</b> km (<b>%d</b> x <b>%d</b> blocs)"):format(
			kmx, kmy, _xmax - _xmin, _ymax - _ymin):gsub("%.0", "")
	else
		text = "Pas d'informations géométriques pour cette carte"
	end

	fs = fs .. "hypertext[0,5.2;4,4;mapinfo;" .. ESC(text) .. "]"

	return fs
end

function formspecs.mapinfo.handle(tabview, fields, name, tabdata)
	if fields.back then
		-- TODO: TEST MAPPER
		if core.mapserver_status() == "running" then
			-- Confirm mapserver stop
			stop_mapserver_confirm(tabview,
				function()
					gamemenu.chosen_map = nil
				end)
			return true
		else
			gamemenu.chosen_map = nil
			return true
		end
	end
end
