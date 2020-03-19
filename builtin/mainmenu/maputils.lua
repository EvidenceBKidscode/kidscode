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

mapmgr = {}

local json_alac_file = "alac.json"

local baseurl = "https://minetest-qualif.ign.fr/rest/public/api/orders/"

local tempfolder = os.tempfolder()

local transcode_status = {
	finished = "ready",
	ongoing = "prepare"
}

local function dlg_mapimport_formspec(data)
	local fs = "size[8,3]"
	if data.field then
		fs = fs .. "label[0,0;" .. (data.message or "") .. "]" ..
			"field[1,1;6,1;dlg_mapimport_formspec_value;;" .. data.field .."]"
	else
		fs = fs .. "label[0,0.5;" .. (data.message or "") .. "]"
	end

	if data.buttons then
		if data.buttons.ok then
			fs = fs .. "button[1,2;2.6,0.5;dlg_mapimport_formspec_ok;" .. data.buttons.ok .. "]"
		end
		if data.buttons.cancel then
			fs = fs .. "button[4,2;2.8,0.5;dlg_mapimport_formspec_cancel;" .. data.buttons.cancel .. "]"
		end
	end

	return fs
end

local function dlg_mapimport_btnhandler(this, fields, data)
	this.parent:show()
	this:hide()
	this:delete()
	if this.data.callbacks then
		for name, cb in pairs(this.data.callbacks) do
			if fields["dlg_mapimport_formspec_" .. name] and
				type(cb) == "function" then
				return cb(this, fields)
			end
		end
	end
	return true
end

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

local function show_question(parent, errmsg, default, cb_ok, cb_cancel)
	local dlg = dialog_create("dlg_mapimport",
		dlg_mapimport_formspec,
		dlg_mapimport_btnhandler,
		nil)

	dlg.data.message = errmsg
	dlg.data.message = errmsg
	dlg.data.field = default or ""
	dlg.data.buttons = {ok = "Valider", cancel = "Annuler"}
	dlg.data.callbacks = {ok = cb_ok, cancel = cb_cancel}
	dlg:set_parent(parent)
	parent:hide()
	dlg:show()
	ui.update()
end

local function dir_exists(path)
	local ok, err, code = os.rename(path, path)
	if not ok and code == 13 then
		return true
	end

	return ok
end

local Map = {}
Map.__index = Map

function Map:new_from_json(json_data)
	local map = {
		demand = true,
		name = "",
		status = transcode_status[json_data.status] or "unknown",
		order_id =  json_data.order_id,
		origin = "ign", -- TODO: Adapt to IGN improvements
		alac = table.copy(json_data),
	}
	setmetatable(map, self)

	if json_data.place and json_data.place ~= "" then
		map.name = json_data.place
	else
		-- TODO : Beter coordinate display (degrees+minutes)
		map.name = json_data.coordinates
	end

	map.alac.requested_by = nil -- Dont store token

	return map
end

function Map:new_from_core_map(core_map_desc)
	assert (core_map_desc and type(core_map_desc) == "table", "Map:new_from_core_map takes a table as argument")
	local map = {
		demand = false,
		name = core_map_desc.name,
		status = "installed",
		origin = "local",
		path = core_map_desc.path,
		gameid = core_map_desc.path, -- Not sure gameid is still usefull
	}
	setmetatable(map, self)

	local file = io.open(map.path .. DIR_DELIM .. json_alac_file, "rb")
	if file then
		local data = file:read("*all")
		file:close()
		local json = minetest.parse_json(data)
		if json and type(json) == "table" then
			map.alac = json
			map.origin = "ign" -- TODO: Adapt to new IGN information
			map.order_id = map.alac.order_id
		end
	end

	return map
end

function Map:is_demand()
	return self.demand
end

function Map:is_map()
	return not self.demand
end

function Map:can_cancel()
	return false -- TODO: Improve when improvement made on json by IGN
end

function Map:can_ask_again()
	return false -- TODO: Improve when improvement made on json by IGN
end

function Map:can_install()
	return self.demand and self.alac and self.alac.package ~= nil
end

local function download_map_demands_list(token)
	local tmpfile = tempfolder .. DIR_DELIM .. os.date("weblist%Y%m%d%H%M%S");
	core.create_dir(tempfolder)

	if not core.download_file(baseurl .. token, tmpfile)
	then
		minetest.log("error", "Unable to download ign.json file.")
		core.delete_dir(tempfolder)
		return
	end

	local file = io.open(tmpfile, "rb")
	if not file then
		minetest.log("error", "Unable to open ign.json file.")
		os.remove(tmpfile)
		core.delete_dir(tempfolder)
		return
	end

	local data = file:read("*all")
	file:close()
	os.remove(tmpfile)
	core.delete_dir(tempfolder)

	local jsonlist = minetest.parse_json(data)
	if jsonlist == nil or type(jsonlist) ~= "table" then
		minetest.log("error", "Failed to parse map demands data")
		return
	end

	local maps = {}
	for _, json in pairs(jsonlist) do
		local map = Map:new_from_json(json)
		if map then
			maps[#maps + 1] = map
		end
	end
	return maps
end

-- Get local map eventually enriched with data from alac.json file
local function get_local_maps()
	local maps = {}
	local core_maps = core.get_worlds()
	for _, map in ipairs(core_maps) do
		local map = Map:new_from_core_map(map)
		if map then
			maps[#maps + 1] = map
		end
	end
	return maps
end

-- Data : token
mapmgr.preparemaplist = function(data)
	local maps = get_local_maps()
	local token = core.settings:get("gartoken")
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

	return maps;
end

mapmgr.compare_map = function (a, b)
	return a and b and a.path == b.path and a.order_id == b.order_id
end

local function save_alac_data(map, destpath)
	local path = destpath or map.path
	if not map.alac or not path then
		return
	end

	path = path .. DIR_DELIM .. "alac.json"

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


-- Launch a function while displaying a status, check for errors
local function async_step(parent, status, async_func, params, ok_func, end_func)
	local dlg = show_status(parent, status)
	core.handle_async(async_func, params,
		function(params)
			dlg:delete()
			if params.log then
				core.log("warning", params.log)
			end
			if params.error then
				core.delete_dir(params.tempfolder)
				show_message(parent, params.error)
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
	local zippath = params.tempfolder .. DIR_DELIM .. os.date("webimport%Y%m%d%H%M%S");
	if not core.download_file(params.map.alac.package, zippath) then
		params.log = "Unable to download url " .. params.map.alac.package ..
			" to " .. tmpfile
		params.error = "Cette carte n'est pas téléchargeable."
	else
		params.zippath = zippath
	end
	return params
end

-- Params:
--   IN  zippath
--   IN  tempfolder
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

	params.unzipedmap = unzippath .. DIR_DELIM .. files[1]
	return params
end

-- Params:
--   IN unzipedmap
--   IN mappath
local function async_install(params)
	if not core.copy_dir(params.unzipedmap, params.mappath) then
		params.log = "Error when copying " .. params.unzipedmap .. " to " .. params.mappath
		params.error = "Erreur lors de l'import, la carte n'a pas été importée."
	end
	return params
end

-- params
-- IN unzipedmap
-- IN tempfolder
local function install_map(parent, params, askname, mapname)
	if askname or not mapname then
		show_question(parent,
			("Choisissez le nom de la carte qui va être importée :"):
			format(core.colorize("#EE0", mapname)), mapname,
			function(this, fields)
				install_map(this, params, false,
					fields.dlg_mapimport_formspec_value)
			end,
			function(this)
				core.delete_dir(params.tempfolder)
			end)
		return
	end

	local mappath = core.get_worldpath() .. DIR_DELIM .. mapname

	if dir_exists(mappath) then
		show_question(parent,
			("Une carte %s existe déja. Choisissez un autre nom :"):
			format(core.colorize("#EE0", mapname)), mapname,
			function(this, fields)
				install_map(this, params, false,
					fields.dlg_mapimport_formspec_value)
			end,
			function(this)
				core.delete_dir(params.tempfolder)
			end)
		return
	end

	params.mappath = mappath

	async_step(parent, "Installation de la carte", async_install, params,
		function(params)
			core.log("info", "New map installed: " .. mapname)
			menudata.worldlist:refresh()
			show_message(parent,
				("La carte %s a bien été importée."):
				format(core.colorize("#EE0", mapname)))
			core.delete_dir(params.tempfolder)
		end
	)
end


function mapmgr.import_map_from_file(parent, zippath)
	local params = { tempfolder = os.tempfolder(), zippath = zippath }
	core.create_dir(params.tempfolder)

	async_step(parent, "Décompression de la carte", async_unzip, params,
		function(params)
			-- Continue common install process now
			install_map(parent, params, true)
		end
	)
	return true
end

function mapmgr.install_map_from_web(parent, map)
	if not map:can_install() then
		show_message(parent, "Cette carte n'est pas téléchargeable.")
		return true
	end

	local params = { tempfolder = os.tempfolder(), map = map }
	core.create_dir(params.tempfolder)

	async_step(parent, "Téléchargement de la carte", async_download, params,
		function(params)
			async_step(parent, "Décompression de la carte", async_unzip, params,
				function(params)
					-- Add json file for tracking
					save_alac_data(params.map, params.unzipedmap)

					-- Continue common install process now
					install_map(parent, params, true, params.map.name)
				end
			)
		end
	)
	return true
end

-- Import web map:
--   local ix = core.settings:get("mainmenu_last_selected_world")
--   local map = menudata.worldlist:get_raw_element(ix)
--   mapmgr.install_map_from_web(parent, map)

function mapmgr.test(parent)
	local ix = core.settings:get("mainmenu_last_selected_world")
	local map = menudata.worldlist:get_raw_element(ix)
	mapmgr.install_map_from_web(parent, map)
end
