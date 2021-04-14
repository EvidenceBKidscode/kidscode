--Minetest / Kidscode
--Copyright (C) 2021 Pierre-Yves Rollo
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

updatemgr = {}

local function fetch_update_info(params)
	local url = minetest.settings:get("ign_update_api_url")
	if not url then
		minetest.log("warning", "No URL in ign_update_api_url settings. Wont check for updates.")
		return
	end

	local tempfolder = params.tempfolder
	local tmpfile = tempfolder .. DIR_DELIM .. os.date("update%Y%m%d%H%M%S");
	core.create_dir(tempfolder)

	if not core.download_file(url, tmpfile)
	then
		-- This situation may be ok if we are offline
		minetest.log("warning", "Unable to get update information from " .. url .. ". Wont check for updates.")
		core.delete_dir(tempfolder)
		return
	end
	
	local file = io.open(tmpfile, "rb")
	if not file then
		minetest.log("error", "Unable to open update information json file. Wont check for updates.")
		core.delete_dir(tempfolder)
		return
	end

	local data = file:read("*all")
	file:close()
	core.delete_dir(tempfolder)

	local info = minetest.parse_json(data)
	if info == nil or type(info) ~= "table" then
		minetest.log("error", "Failed to parse update information json file. Wont check for updates.")
		return
	end

	return info
end

local update_info

local function display_update_info(info)
	if not info then
		return
	end
	update_info = info
	ui.update()
end

function updatemgr.get_update_info()
	local version = core.get_kidscode_version_string()

	if not update_info then
		return
	end

	if update_info.version == version then
		-- Version is ok
		return "hypertext[0.2,7.5;5,0.5;update;<center><style size=12 color=#888>Votre version est à jour</style></center>]"		
	elseif update_info.version > version then
		-- Version is outdated
		return 
			"box[0.2,5.9;5,1.9;#4C4]" ..
			"hypertext[0.3,6;4.8,1;update;" ..
			"<center><b>Une nouvelle version de Kidscode est disponible !</b></center>]" ..
			"button[0.3,7;4.8,0.7;btn_update;Mettre à jour Kidscode]"
	else
		-- Version is newer
		return 
			"box[0.2,6;5,1.9;#888]" ..
			"hypertext[0.3,6.1;4.8,1.7;update;" ..
			"<center><style size=14>Votre version de Kidscode est en avance sur la version officielle. " ..
			"Cela peut éventuellement causer des problèmes</style></center>]"
	end
end

function updatemgr.handle_buttons(fields)
	if not fields.btn_update then
		return
	end
	
	if update_info and update_info.file then
		core.launch_browser(update_info.file)
		os.exit(0)
	end
end

local tempfolder = os.tempfolder()
core.handle_async(
	fetch_update_info,
	{ tempfolder = tempfolder },
	display_update_info)
