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
	if this.data.callbacks then
		for name, cb in pairs(this.data.callbacks) do
			if fields["dlg_mapimport_formspec_" .. name] and
				type(cb) == "function" then
				return cb(this, fields)
			end
		end
	end
	this.parent:show()
	this:hide()
	this:delete()
	return true
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
		status = "prepare", -- TODO: Adapt to IGN improvements
		origin = "ign", -- TODO: Adapt to IGN improvements
		alac = table.copy(json_data)
	}
	setmetatable(map, self)

	if json_data.place and json_data.place ~= "" then
		map.name = json_data.place
	else
		-- TODO : Beter coordinate display (degrees+minutes)
		map.name = json_data.coordinates
	end
	if json_data.package then -- TODO: Adapt to IGN improvements
		map.status = "ready"
	end

	map.order_id = map.alac.order_id
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

function Map:save_alac_data(destpath)
	local path = destpath or self.path
	if not self.alac or not path then
		return
	end

	path = path .. DIR_DELIM .. "alac.json"

	local file = io.open(path, "wb")
	if  not file then
		minetest.log("error", "Unable to open " .. path .. " for writing.")
		return
	end

	if file then
		local data = minetest.write_json(self.alac)
		if not data then
			minetest.log("error", "Unable to encode json data for map.")
			return
		end
		file:write(data)
		io.close(file)
	end
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
	local tmpfile = os.tmpname()
	if not core.download_file(baseurl .. token, tmpfile)
	then
		minetest.log("error", "Unable to download ign.json file.")
		return
	end

	local file = io.open(tmpfile, "rb")
	if not file then
		minetest.log("error", "Unable to open ign.json file.")
		os.remove(tmpfile)
		return
	end

	local data = file:read("*all")
	file:close()
	os.remove(tmpfile)

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

local function unzip_map(zippath)
	local zipname = zippath:match("([^" .. DIR_DELIM .. "]*)$")

	-- Create a temp directory for decompressing zip file in it
	local tempdir = os.tempfolder() .. DIR_DELIM .. os.date("mapimport%Y%m%d%H%M%S");
	core.create_dir(tempdir)

	-- Extract archive
	if not core.extract_zip(zippath, tempdir) then
		core.delete_dir(tempdir)
		return { log = "Unable to extract zipfile " .. zippath .. " to " .. tempdir,
			error = "Impossible d'installer la carte : Impossible d'ouvrir l'archive." }
	end

	-- Check content
	local files = core.get_dir_list(tempdir, true)
	if (files == nil or #files == 0) then
		core.delete_dir(tempdir)
		return { log = "No directory found in "..zippath,
			error = "Impossible d'installer la carte : Aucun dossier dans l'archive." }
	end

	if #files ~= 1 then
		core.delete_dir(tempdir)
		return { log = "Too many directories in "..zippath,
			error = "Impossible d'installer la carte : L'archive ne doit contenir qu'un seul dossier." }
	end

	return { dir = tempdir .. DIR_DELIM .. files[1] }
end

local function install_map(parent, tempdir, askname, mapname)
	if askname or not mapname then
		show_question(parent,
			("Choisissez le nom de la carte qui va être importée :"):
			format(core.colorize("#EE0", mapname)), mapname,
			function(this, fields)
				--TODO: Add os.remove(tmpfile) on cancel ?
				return install_map(this, tempdir, false, fields.dlg_mapimport_formspec_value)
			end,
			function(this)
				this.parent:show()
				this:hide()
				this:delete()
				return true
			end)
		return true
	end

	local mappath = core.get_worldpath() .. DIR_DELIM .. mapname

	if dir_exists(mappath) then
		show_question(parent,
			("Une carte %s existe déja. Choisissez un autre nom :"):
			format(core.colorize("#EE0", mapname)), mapname,
			function(this, fields)
				return install_map(this, tempdir, false, fields.dlg_mapimport_formspec_value)
			end,
			function(this)
				this.parent:show()
				this:hide()
				this:delete()
				return true
			end)
		return true
	end

	if core.copy_dir(tempdir, mappath) then
		core.log("info", "New map installed: " .. mapname)
		menudata.worldlist:refresh()
		show_message(parent,
			("La carte %s a bien été importée."):
			format(core.colorize("#EE0", mapname)))
	else
		core.log("error", "Error when copying " .. tempdir .. " to " .. mappath)
		show_message(parent,
			"Erreur lors de l'import, la carte n'a pas été importée.")
	end
	core.delete_dir(tempdir)

	return true
end


function mapmgr.install_map_from_web(parent_dlg, map)

	if not map:can_install() then
		show_message(parent_dlg, "Cette carte n'est pas téléchargeable.")
		return true
	end

	local tmpfile = os.tmpname()

	if not core.download_file(map.alac.package, tmpfile) then
		show_message(parent_dlg, "Impossible de télécharger la carte.")
		return true
	end

	local result = unzip_map(tmpfile)
	os.remove(tmpfile)

	if result.error then
		core.log("warning", result.log)
		show_message(parent_dlg, result.error)
		return true
	end

	-- Add json file for tracking
	map:save_alac_data(result.dir)

	-- Install map (ie copy unique folder to world dir)
	return install_map(parent_dlg, result.dir, true, map.name)
end

function mapmgr.import_map_from_file(parent_dlg, zippath)
	local zipname = zippath:match("([^" .. DIR_DELIM .. "]*)$")

	local result = unzip_map(tmpfile)

	if result.error then
		core.log("warning", result.log)
		show_message(parent_dlg, result.error)
		return true
	end

	-- Install map (ie copy unique folder to world dir)
	return install_map(parent_dlg, result.dir, true)
end
