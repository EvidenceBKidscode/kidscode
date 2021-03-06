--Minetest
--Copyright (C) 2020 Pierre-Yves Rollo
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

-- The real identifiers of maps are order_id and path.

mapmgr = { errors = {} }

local json_alac_file = "alac.json"
local json_geo_file  = "geometry.dat"

local baseurl = minetest.settings:get("ign_map_api_url") or ""

local function file_exists(path)
	local ok, err, code = os.rename(path, path)
	return ok or code == 13
end

local function check_map_name(name)
	if name then
		if name:find("[^%w%s-_!#$&\"'()+,.;=@^]+") then
			return "Le nom ne doit comporter ni accents, ni caractères spéciaux."
		end
		if file_exists(core.get_worldpath() .. DIR_DELIM .. name) then
			return ("Une carte \"%s\" existe déja."):format(name)
		end
	end
end

--------------------------------------------------------------------------------
-- Maps and map demands management
--------------------------------------------------------------------------------

local function add_data_from_json(map, json_data)
	map.alac = table.copy(json_data)
	map.alac.requested_by = nil

	map.order_id = json_data.order_id
	map.filesize = json_data.fileSize
	map.mapsize  = json_data.emprise
	map.origin   = json_data.origin or
		(map.order_id and -- Old way origin was built
			("GAR / " .. (json_data.data_source_topo == 1 and "OSM" or "IGN")) or
			"Locale")
end

local transcode_status = {
	finished = "ready",
	ongoing = "prepare",
	pending = "prepare"
}

function mapmgr.new_map_from_json(json_data)
	local map = {
		demand = true,
		name = "",
		status = transcode_status[json_data.status] or "unknown",
	}

	add_data_from_json(map, json_data)

	-- Name building
	-- TODO : Beter coordinate display (degrees+minutes)
	map.name = json_data.place or json_data.coordinates or ""

	if json_data.snow_height_max and json_data.snow_height_max > 0 then
		map.name = map.name .. " (neige)"
	end

	return map
end

function mapmgr.new_map_from_core_world(core_map_desc)
	assert (core_map_desc and type(core_map_desc) == "table", "Map:new_from_core_map takes a table as argument")
	local map = {
		demand = false,
		name = core_map_desc.name,
		status = "installed",
		path = core_map_desc.path,
		origin = "Locale",
		gameid = core_map_desc.gameid, -- Not sure gameid is still usefull
		coreindex = core_map_desc.coreindex, -- Not sure gameid is still usefull
	}

	local file = io.open(map.path .. DIR_DELIM .. json_alac_file, "rb")
	if file then
		local data = file:read("*all")
		file:close()
		local json = minetest.parse_json(data)
		if json and type(json) == "table" then
			add_data_from_json(map, json)
		end
	end

	return map
end

function mapmgr.map_is_demand(map)
	return map and map.demand
end

function mapmgr.map_is_map(map)
	return map and not map.demand
end

function mapmgr.can_cancel_map(map)
	return false -- TODO: Improve when improvement made on json by IGN
end

function mapmgr.can_ask_map_again(map)
	return false -- TODO: Improve when improvement made on json by IGN
end

function mapmgr.can_install_map(map)
	return map and map.demand and map.alac and map.alac.package ~= nil
end

local function save_map_alac_data(map, destpath)
	local path = destpath or map.path
	if not map.alac or not path then
		return
	end

	path = path .. DIR_DELIM .. json_alac_file

	local file = io.open(path, "wb")
	if  not file then
		minetest.log("error", "Unable to open " .. path .. " for writing.")
		return
	end

	if file then
		local data = minetest.write_json(map.alac)
		if not data then
			minetest.log("error", "Unable to encode json data for map.")
			return
		end
		file:write(data)
		io.close(file)
	end
end

local function download_map_demands_list(token)
	local tempfolder = os.tempfolder()
	local tmpfile = tempfolder .. DIR_DELIM .. os.date("weblist%Y%m%d%H%M%S");
	core.create_dir(tempfolder)

	if not core.download_file(baseurl .. token, tmpfile)
	then
		minetest.log("error", "Unable to download ign.json file.")
		mapmgr.errors.network = true
		core.delete_dir(tempfolder)
		return
	end
	mapmgr.errors.network = nil

	local file = io.open(tmpfile, "rb")
	if not file then
		minetest.log("error", "Unable to open ign.json file.")
		mapmgr.errors.file = true
		core.delete_dir(tempfolder)
		return
	end
	mapmgr.errors.file = nil

	local data = file:read("*all")
	file:close()
	core.delete_dir(tempfolder)

	local jsonlist = minetest.parse_json(data)
	if jsonlist == nil or type(jsonlist) ~= "table" then
		minetest.log("error", "Failed to parse map demands data")
		mapmgr.errors.format = true
		return
	end
	mapmgr.errors.format = nil

	local maps = {}
	for _, json in pairs(jsonlist) do
		local map = mapmgr.new_map_from_json(json)
		if map then
			maps[#maps + 1] = map
		end
	end

	return maps
end

-- Get local map eventually enriched with data from alac.json file
local function get_local_maps()
	local maps = {}
	local worlds = core.get_worlds()
	for index, world in ipairs(worlds) do
		world.coreindex = index
		local map = mapmgr.new_map_from_core_world(world)
		if map then
			maps[#maps + 1] = map
		end
	end

	return maps
end

local function create_uid(map)
	if map.uid then
		return
	end
	if map.order_id then
		map.uid =  "order" .. map.order_id
	else
		map.alac = map.alac or {}
		if not map.alac.uid then
			map.alac.uid = "local" ..
				(map.order_id or math.random(0, 9999999999))
			save_map_alac_data(map)
		end
		map.uid = map.alac.uid
	end
end

local function get_info_from_mapservice()
	local mapservice = core.volatile_settings:get("mapservice")
	if not mapservice then
		return nil, nil
	end

	local remains, origin, token =
			mapservice:match("^(.-)([^/]-)/?([^/]-)/?$")

	-- Legacy, no origin, token at the first and only place
	if not token or token == "" then
		return origin, nil
	end

	return token, origin
end

local function get_origin()
	local token, origin = get_info_from_mapservice()
	return origin
end

local function get_token()
	local token, origin = get_info_from_mapservice()
	return token
end

function mapmgr.preparemaplist(data)
	local maps = get_local_maps()
	local token = get_token()

	if token then
		local demands = download_map_demands_list(token)
		for _, demand in ipairs(demands or {}) do
			if demand.order_id then
				local already_listed = false
				for _, map in ipairs(maps) do
					if map.order_id == demand.order_id then
						already_listed = true
						break
					end
				end
				if not already_listed then
					maps[#maps + 1] = demand
				end
			end
		end
	end

	-- Ensure all maps have UIDs
	for _, map in ipairs(maps) do
		create_uid(map)
	end

	return maps;
end

function mapmgr.compare_map(a, b)
	return a and b and a.path == b.path and a.order_id == b.order_id
end

function mapmgr.get_geometry(map)
	if not mapmgr.map_is_map(map) then
		return false --Not an installed map
	end

	if map.geo then
		return true -- Already read
	end

	local filename = map.path .. DIR_DELIM .. json_geo_file

	local file = io.open(filename, "rb")
	if not file then
		return false -- No geometry data found
	end

	local geostring = file:read("*all")
	file:close()
	local data = minetest.parse_json(geostring)

	if data == nil or type(data) ~= "table" then
		minetest.log("error",
			string.format("Unable to parse content of file %s.", filename))
		return false
	end

	map.geo = data
	return true
end

--------------------------------------------------------------------------------
-- GUI management
--------------------------------------------------------------------------------

-- Multi purpose dialog (status, message, question)
local function dlg_mapimport_formspec(data)
	local fs = "formspec_version[3]"
	local y = data.message:find "\n" and 2.1 or 1.7

	if data.field then
		fs = fs .. "size[7," .. (y + 0.9) .. "]" ..
			"hypertext[0.2,0.3;6.6,1;;" .. core.formspec_escape(data.message or "") .. "]" ..
			"field[0.25," .. (y - 0.9) ..
				";6.5,0.7;dlg_mapimport_formspec_value;;" .. data.field .."]"
	else
		fs = fs .. "size[7,3]" ..
			"hypertext[0.2,0.5;6.6,1;;" .. core.formspec_escape(data.message or "") .. "]"
	end

	if data.buttons then
		local x = (data.buttons.ok and data.buttons.cancel) and 0.5 or 2.25

		if data.buttons.ok then
			fs = fs .. "button[" .. x .. "," .. y ..
				";2.5,0.7;dlg_mapimport_formspec_ok;" .. data.buttons.ok .. "]"
			x = x + 3.5
		end
		if data.buttons.cancel then
			fs = fs .. "button[" .. x .. "," .. y ..
				";2.5,0.7;dlg_mapimport_formspec_cancel;" .. data.buttons.cancel .. "]"
		end
	end

	return fs
end

-- Generic button handler: close dialog and call callback if exists
local function dlg_mapimport_btnhandler(this, fields, data)
	this:delete()
	if this.data.callbacks then
		for name, cb in pairs(this.data.callbacks) do
			if fields["dlg_mapimport_formspec_" .. name] and
					type(cb) == "function" then
				return cb(this, fields)
			end
		end
		if fields["key_enter"] and this.data.callbacks.enter and
				type(this.data.callbacks.enter) then
			return this.data.callbacks.enter(this, fields)
		end
	end
	return true
end

-- Display status dialog
local function show_status(parent, msg)
	local dlg = dialog_create("dlg_mapimport",
		dlg_mapimport_formspec,
		dlg_mapimport_btnhandler,
		nil)
	dlg.data.message = msg
	dlg.data.buttons = {}
	dlg:set_parent(parent)
	parent:hide()
	dlg:show()
	ui.update()
	return dlg
end

-- Display message dialog
local function show_message(parent, errmsg)
	local dlg = dialog_create("dlg_mapimport",
		dlg_mapimport_formspec,
		dlg_mapimport_btnhandler,
		nil)
	dlg.data.message = errmsg
	dlg.data.buttons = {ok = "OK"}
	dlg:set_parent(parent)
	parent:hide()
	dlg:show()
	ui.update()
end

-- Display question dialog
local function show_question(parent, errmsg, default, cb_ok, cb_cancel)
	local dlg = dialog_create("dlg_mapimport",
		dlg_mapimport_formspec,
		dlg_mapimport_btnhandler,
		nil)

	dlg.data.message = errmsg
	dlg.data.field = default or ""
	dlg.data.buttons = {ok = "Valider", cancel = "Annuler" }
	dlg.data.callbacks = {ok = cb_ok, cancel = cb_cancel, enter = cb_ok }
	dlg:set_parent(parent)
	parent:hide()
	dlg:show()
	ui.update()
end

local function wait(params)
	local time = os.time()
--	while (os.time() == time) do
--	end
	return params
end

local inprogress = nil

local function refresh_progress(params)
	if inprogress then
		ui.delete(inprogress.dlg)
		inprogress.dlg = show_status(inprogress.parent,
				inprogress.status .. "\n" .. inprogress.func(params)),
		core.handle_async(wait, params, refresh_progress)
	end
end

local function async_step_progress(parent, status, async_func, params, progress_func, ok_func)
	inprogress = {
		parent = parent,
		status = status,
		dlg = show_status(parent, status),
		func = progress_func,
	}
	refresh_progress(params)

	core.handle_async(async_func, params,
		function(params)
			-- Cand use :delete dialog method, metadata lost by handle_async
			inprogress.parent.hidden = false
			ui.delete(inprogress.dlg)
			inprogress = nil

			if params.log then
				core.log("warning", params.log)
			end
			if params.error then
				core.delete_dir(params.tempfolder)
				show_message(parent, "<b><center><style color=yellow>" .. params.error)
			else
				if ok_func and type(ok_func) == "function" then
					ok_func(params)
				end
			end
		end
	)
end

-- Launch a function while displaying a status, check for errors
local function async_step(parent, status, async_func, params, ok_func)
	local dlg = show_status(parent, status)
	core.handle_async(async_func, params,
		function(params)
			dlg:delete()
			if params.log then
				core.log("warning", params.log)
			end
			if params.error then
				core.delete_dir(params.tempfolder)
				show_message(parent, "<b><center><style color=yellow>" .. params.error)
			else
				if ok_func and type(ok_func) == "function" then
					ok_func(params)
				end
			end
		end
	)
end

-- Params:
--   IN  tempfolder
--   OUT zippath
local function async_download(params)
	if not core.download_file(params.map.alac.package, params.zippath) then
		params.log = "Unable to download url " .. params.map.alac.package
		params.error = "Cette carte n'est pas téléchargeable."
	end
	return params
end

-- Params:
--   IN  zippath
--   IN  tempfolder
--   IN  token
--   OUT unzipedmap
local function async_unzip(params)
	-- Create a temp directory for decompressing zip file in it
	local unzippath = params.tempfolder .. DIR_DELIM .. os.date("mapimport%Y%m%d%H%M%S");
	core.create_dir(unzippath)

	-- Extract archive
	if not core.extract_zip(params.zippath, unzippath) then
		params.log = "Unable to extract zipfile " .. zippath ..
			" to " .. unzippath
		params.error = "Impossible d'installer la carte : Impossible d'ouvrir l'archive."
		return params
	end

	-- Check content
	local files = core.get_dir_list(unzippath, true)
	if (files == nil or #files == 0) then
		params.log = "No directory found in " .. params.zippath .. " (1 expected)"
		params.error = "Impossible d'installer la carte : Aucun dossier dans l'archive."
		return params
	end

	if #files ~= 1 then
		params.log = "Too many directories in " .. params.zippath .. " (1 expected)"
		params.error = "Impossible d'installer la carte : L'archive ne doit contenir qu'un seul dossier."
		return params
	end

	if params.map then
		-- Tell map server that map have been downloaded
		local url = ("%s%s/%s/recv"):format(
				minetest.settings:get("ign_map_api_url") or "",
				params.token, params.map.alac.order_id)

		if core.download_file(url, params.tempfolder .. DIR_DELIM .. "recv.tmp") then
			minetest.log("action",
				("Told server that map %s has been downloaded."):format(params.map.alac.order_id))
		else
			minetest.log("error",
				("Could tell server map has been downloaded (error reaching %s)."):
				format(url))
		end
	end

	params.unzipedmap = unzippath .. DIR_DELIM .. files[1]
	return params
end

-- Params:
--   IN unzipedmap
local function async_verify(params)

	-- Add current origin to map json file
	local json_file = params.unzipedmap .. DIR_DELIM .. "alac.json"

	local file = io.open(json_file, "rb")
	if not file then
		-- No file, nothing to do (probably a local map)
		return params
	end

	local data = file:read("*all")
	file:close()
	local json = minetest.parse_json(data)

	if not json or type(json) ~= "table" then
		params.log = "Unable to parse " .. json_file .. " for adding origin."
		params.error = "Erreur lors de l'installation, mais la carte a été importée."
		return params
	end

	-- Add origin information to map json file
	json.origin = params.origin

	local file = io.open(json_file, "wb")
	if  not file then
		params.log = "Unable to open " .. json_file .. " for writing after adding origin."
		params.error = "Erreur lors de l'installation, mais la carte a été importée."
		return params
	end

	local data = minetest.write_json(json)
	if not data then
		file:close()
--		io.close(file)
		params.log = "Unable to encode json data after adding origin."
		params.error = "Erreur lors de l'installation, mais la carte a été importée."
		return params
	end

	file:write(data)
	file:close()
--	io.close(file)

	if not core.verify_world(params.unzipedmap) then
		params.log = "Error when verifying world " .. params.unzipedmap
		params.error = "Erreur lors de la vérification, la carte n'a pas été importée."
	end
	return params
end

-- Params:
--   IN unzipedmap
--   IN mappath
local function async_install(params)
	if not core.copy_dir(params.unzipedmap, params.mappath) then
		params.log = "Error when copying " .. params.unzipedmap .. " to " .. params.mappath
		params.error = "Erreur lors de l'installation, la carte n'a pas été importée."
	end

	return params
end

local function get_question(params, askname, mapname)
	if askname or not mapname then
		return "<b>Choisissez le nom de la carte qui va être importée :</b>"
	end

	local error = check_map_name(mapname)

	if error then
		return "<style color=yellow>" .. error .. "</style>\n<b>Choisissez un autre nom :</b>"
	end
end

-- params
-- IN unzipedmap
-- IN tempfolder
local function install_map(parent, params, askname, mapname)
	local question = get_question(params, askname, mapname)
	if question then
		show_question(parent, question, mapname,
			function(this, fields)
				install_map(this.parent, params, false,
					fields.dlg_mapimport_formspec_value)
			end,
			function(this)
				ui.update()
				core.delete_dir(params.tempfolder)
			end)
		return
	end

	params.mappath = core.get_worldpath() .. DIR_DELIM .. mapname
	params.token = get_token()
	params.origin = get_origin()

	async_step(parent, "<b><center>Vérification et conversion de la carte", async_verify, params,
		function(params)
			async_step(parent, "<b><center>Installation de la carte", async_install, params,
				function(params)
					core.log("info", "New map installed: " .. mapname)
					menudata.worldlist:refresh()
					show_message(parent,
						("<b><center>La carte \"<style color=yellow>%s</style>\" a bien été importée."):
						format(mapname))
					core.delete_dir(params.tempfolder)

					-- Choose map if imported from file
					local map
					for index, world in ipairs(core.get_worlds()) do
						if world.name == mapname then
							world.coreindex = index
							map = mapmgr.new_map_from_core_world(world)
							create_uid(map)
							core.settings:set("mainmenu_last_selected_world_uid", map.uid)
						end
					end

					if map and not map.order_id then
						gamemenu.chosen_map = map
					end
				end
			)
		end
	)
end

-- Import a map from a file. Call it and return, the rest of the process is async
function mapmgr.import_map_from_file(parent, zippath)
	local params = { tempfolder = os.tempfolder(), zippath = zippath }
	core.create_dir(params.tempfolder)

	async_step(parent, "<b><center>Décompression de la carte", async_unzip, params,
		function(params)
			-- Continue common install process now
			install_map(parent, params, true)
		end
	)
end

function fsize(path)
	local size = 0
	local file = io.open(path, "rb")
	if file then
		size = file:seek("end")
		file:close()
	end
	return size
 end

-- Import a map from web. Call it and return, the rest of the process is async
function mapmgr.install_map_from_web(parent, map)
	if not mapmgr.can_install_map(map) then
		show_message(parent, "Cette carte n'est pas téléchargeable.")
		return
	end

	local params = { tempfolder = os.tempfolder(), map = map }
	params.zippath = params.tempfolder .. DIR_DELIM .. os.date("webimport%Y%m%d%H%M%S");

	core.create_dir(params.tempfolder)

	async_step_progress(parent, "<b><center>Téléchargement de la carte", async_download, params,
		function(params)
			local dlsize = fsize(params.zippath)
			if map.filesize and map.filesize ~= 0 then
				return ("%s (%.1f%%)"):format(humanReadableSize(dlsize),
					dlsize / map.filesize * 100)
			else
				return humanReadableSize(dlsize)
			end
		end,
		function(params)
			async_step(parent, "<b><center>Décompression de la carte", async_unzip, params,
				function(params)
					-- Add json file for tracking
					save_map_alac_data(params.map, params.unzipedmap)

					-- Continue common install process now
					install_map(parent, params, true, stripAccents(params.map.name))
				end
			)
		end
	)
end

-- Rename a map (in newname omitted or incorrect, user is prompted for a name)
function mapmgr.rename_map(parent, map, newname)

	local question

	-- Checks
	if not newname then
		question = "<b>Renommer la carte</b>"
	elseif newname == map.name then
		return -- Nothing to do
	else
		local error = check_map_name(newname)

		if error then
			question = "<b>Renommer la carte</b>\n<style color=yellow>" .. error .. "</style>"
		end
	end

	-- Ask or act
	if question then
		show_question(parent, question, newname or map.name,
			function(this, fields)
				mapmgr.rename_map(this.parent, map, fields.dlg_mapimport_formspec_value)
				return true
			end
		)
	else
		local ok, err, code = os.rename(
			core.get_worldpath() .. DIR_DELIM .. map.name,
			core.get_worldpath() .. DIR_DELIM .. newname)
		menudata.worldlist:refresh()
	end
end
