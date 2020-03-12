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


local Map = {}
Map.__index = Map

function Map:new_from_json(json_data)
	map = {
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

function Map:save_alac_data()
	if not self.alac or not self.path then
		return
	end

	local path = self.path .. DIR_DELIM .. "alac.json"
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
	return self.json.package ~= nil
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
	if data.token then
		local demands = download_map_demands_list(data.token)
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
