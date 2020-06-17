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
-- Global menu data
--------------------------------------------------------------------------------
menudata = {}

--------------------------------------------------------------------------------
-- Local cached values
--------------------------------------------------------------------------------
local min_supp_proto, max_supp_proto

function common_update_cached_supp_proto()
	min_supp_proto = core.get_min_supp_proto()
	max_supp_proto = core.get_max_supp_proto()
end
common_update_cached_supp_proto()
--------------------------------------------------------------------------------
-- Menu helper functions
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
local function render_client_count(n)
	if     n > 99 then return '99+'
	elseif n >= 0 then return tostring(n)
	else return '' end
end

local function configure_selected_world_params(idx)
	local worldconfig = pkgmgr.get_worldconfig(menudata.worldlist:get_list()[idx].path)
	if worldconfig.creative_mode then
		core.settings:set("creative_mode", worldconfig.creative_mode)
	end
	if worldconfig.enable_damage then
		core.settings:set("enable_damage", worldconfig.enable_damage)
	end
end

--------------------------------------------------------------------------------
function image_column(tooltip, flagname)
	return "image,tooltip=" .. ESC(tooltip) .. "," ..
		"0=" .. ESC(defaulttexturedir .. "blank.png") .. "," ..
		"1=" .. ESC(defaulttexturedir ..
			(flagname and "server_flags_" .. flagname .. ".png" or "blank.png")) .. "," ..
		"2=" .. ESC(defaulttexturedir .. "server_ping_4.png") .. "," ..
		"3=" .. ESC(defaulttexturedir .. "server_ping_3.png") .. "," ..
		"4=" .. ESC(defaulttexturedir .. "server_ping_2.png") .. "," ..
		"5=" .. ESC(defaulttexturedir .. "server_ping_1.png")
end

--------------------------------------------------------------------------------
function order_favorite_list(list)
	local res = {}
	--orders the favorite list after support
	for i = 1, #list do
		local fav = list[i]
		if is_server_protocol_compat(fav.proto_min, fav.proto_max) then
			res[#res + 1] = fav
		end
	end
	for i = 1, #list do
		local fav = list[i]
		if not is_server_protocol_compat(fav.proto_min, fav.proto_max) then
			res[#res + 1] = fav
		end
	end
	return res
end

--------------------------------------------------------------------------------
function render_serverlist_row(spec)
	local text = ""
	--[[
		It is possible to add icons according to which kind of connection is
		possible :

	if spec.gameserver then
		text = text .. "J,"
	else
		text = text .. ","
	end
	if spec.mapserver then
		text = text .. "C,"
	else
		text = text .. ","
	end
	]]

	if spec.name then
		text = text .. ESC(spec.name:trim())
	elseif spec.address then
		text = text .. spec.address:trim()
		if spec.port then
			text = text .. ":" .. spec.port
		end
	end

	return text
end

--------------------------------------------------------------------------------
os.tempfolder = function()
	if core.settings:get("TMPFolder") then
		return core.settings:get("TMPFolder") .. DIR_DELIM .. "MT_" .. math.random(0,10000)
	end

	local filetocheck = os.tmpname()
	os.remove(filetocheck)

	-- luacheck: ignore
	-- https://blogs.msdn.microsoft.com/vcblog/2014/06/18/c-runtime-crt-features-fixes-and-breaking-changes-in-visual-studio-14-ctp1/
	--   The C runtime (CRT) function called by os.tmpname is tmpnam.
	--   Microsofts tmpnam implementation in older CRT / MSVC releases is defective.
	--   tmpnam return values starting with a backslash characterize this behavior.
	-- https://sourceforge.net/p/mingw-w64/bugs/555/
	--   MinGW tmpnam implementation is forwarded to the CRT directly.
	-- https://sourceforge.net/p/mingw-w64/discussion/723797/thread/55520785/
	--   MinGW links to an older CRT release (msvcrt.dll).
	--   Due to legal concerns MinGW will never use a newer CRT.
	--
	--   Make use of TEMP to compose the temporary filename if an old
	--   style tmpnam return value is detected.
	if filetocheck:sub(1, 1) == "\\" then
		local tempfolder = os.getenv("TEMP")
		return tempfolder .. filetocheck
	end

	local randname = "MTTempModFolder_" .. math.random(0,10000)
	local backstring = filetocheck:reverse()
	return filetocheck:sub(0, filetocheck:len() - backstring:find(DIR_DELIM) + 1) ..
		randname
end

--------------------------------------------------------------------------------

local status_translate = {
	installed = "Installée",
	cancelled = "Annulée",
	prepare   = "En préparation",
	ready     = "Prête",
}

local origin_translate = {
	ign       = "GAR / IGN",
	["local"] = "Local",
}

local status_color = {
	installed = "#00ff00",
	cancelled = "#ff0000",
	prepare   = "#ffa500",
	ready     = "#ffff00",
}

function menu_render_worldlist()
	local retval = ""
	local current_worldlist = menudata.worldlist:get_list()

	for i, v in ipairs(current_worldlist) do
		if retval ~= "" then retval = retval .. "," end

		retval = retval ..
			"#ffffff" .. "," ..
			ESC(v.name):sub(1,50) .. "," ..
			"#ffffff" .. "," ..
			ESC(v.alac and v.alac.delivered_on and v.alac.delivered_on:match("%S*") or "") .. "," ..
			"#ffffff" .. "," ..
			ESC(origin_translate[v.origin] or "?") .. "," ..
			(status_color[v.status] or "#ffffff") .. "," ..
			ESC(status_translate[v.status] or "?")
	end

	return retval
end

--------------------------------------------------------------------------------
function menu_handle_key_up_down(fields, textlist, settingname)
	local oldidx, newidx = core.get_textlist_index(textlist), 1
	if fields.key_up or fields.key_down then
		if fields.key_up and oldidx and oldidx > 1 then
			newidx = oldidx - 1
		elseif fields.key_down and oldidx and
				oldidx < menudata.worldlist:size() then
			newidx = oldidx + 1
		end
		core.settings:set(settingname, menudata.worldlist:get_raw_index(newidx))
		configure_selected_world_params(newidx)
		return true
	end
	return false
end

--------------------------------------------------------------------------------
function asyncOnlineFavourites()
	if not menudata.public_known then
		menudata.public_known = {{
			name = fgettext("Loading..."),
			description = fgettext_ne("Try reenabling public serverlist and check your internet connection.")
		}}
	end
	menudata.favorites = menudata.public_known
	menudata.favorites_is_public = true

	if not menudata.public_downloading then
		menudata.public_downloading = true
	else
		return
	end

	core.handle_async(
		function(param)
			return core.get_favorites("online")
		end,
		nil,
		function(result)
			menudata.public_downloading = nil
			local favs = order_favorite_list(result)
			if favs[1] then
				menudata.public_known = favs
				menudata.favorites = menudata.public_known
				menudata.favorites_is_public = true
			end
			core.event_handler("Refresh")
		end
	)
end

--------------------------------------------------------------------------------
function text2textlist(xpos, ypos, width, height, tl_name, textlen, text, transparency)
	local textlines = core.wrap_text(text, textlen, true)
	local retval = "textlist[" .. xpos .. "," .. ypos .. ";" .. width ..
			"," .. height .. ";" .. tl_name .. ";"

	for i = 1, #textlines do
		textlines[i] = textlines[i]:gsub("\r", "")
		retval = retval .. ESC(textlines[i]) .. ","
	end

	retval = retval .. ";0;"
	if transparency then retval = retval .. "true" end
	retval = retval .. "]"

	return retval
end

--------------------------------------------------------------------------------
function is_server_protocol_compat(server_proto_min, server_proto_max)
	if (not server_proto_min) or (not server_proto_max) then
		-- There is no info. Assume the best and act as if we would be compatible.
		return true
	end
	return min_supp_proto <= server_proto_max and max_supp_proto >= server_proto_min
end
--------------------------------------------------------------------------------
function is_server_protocol_compat_or_error(server_proto_min, server_proto_max)
	if not is_server_protocol_compat(server_proto_min, server_proto_max) then
		local server_prot_ver_info, client_prot_ver_info
		local s_p_min = server_proto_min
		local s_p_max = server_proto_max

		if s_p_min ~= s_p_max then
			server_prot_ver_info = fgettext_ne("Server supports protocol versions between $1 and $2. ",
				s_p_min, s_p_max)
		else
			server_prot_ver_info = fgettext_ne("Server enforces protocol version $1. ",
				s_p_min)
		end
		if min_supp_proto ~= max_supp_proto then
			client_prot_ver_info= fgettext_ne("We support protocol versions between version $1 and $2.",
				min_supp_proto, max_supp_proto)
		else
			client_prot_ver_info = fgettext_ne("We only support protocol version $1.", min_supp_proto)
		end
		gamedata.errormessage = fgettext_ne("Protocol version mismatch. ")
			.. server_prot_ver_info
			.. client_prot_ver_info
		return false
	end

	return true
end
--------------------------------------------------------------------------------
function menu_worldmt(selected, setting, value)
	local world = menudata.worldlist:get_list()[selected]
	if world and world.path then
		local filename = world.path .. DIR_DELIM .. "world.mt"
		local world_conf = Settings(filename)

		if value then
			if not world_conf:write() then
				core.log("error", "Failed to write world config file")
			end
			world_conf:set(setting, value)
			world_conf:write()
		else
			return world_conf:get(setting)
		end
	end
end

function menu_worldmt_legacy(selected)
	local modes_names = {"creative_mode", "enable_damage", "server_announce"}
	for _, mode_name in pairs(modes_names) do
		local mode_val = menu_worldmt(selected, mode_name)
		if mode_val then
			core.settings:set(mode_name, mode_val)
		else
			menu_worldmt(selected, mode_name, core.settings:get(mode_name))
		end
	end
end

local accents = {
	["À"] = "A", ["Á"] = "A", ["Â"] = "A", ["Ã"] = "A",
	["Ä"] = "A", ["Å"] = "A", ["Æ"] = "AE", ["Ç"] = "C",
	["È"] = "E", ["É"] = "E", ["Ê"] = "E", ["Ë"] = "E",
	["Ì"] = "I", ["Í"] = "I", ["Î"] = "I", ["Ï"] = "I",
	["Ð"] = "D", ["Ñ"] = "N", ["Œ"] = "OE",
	["Ò"] = "O", ["Ó"] = "O", ["Ô"] = "O",
	["Õ"] = "O", ["Ö"] = "O", ["Ø"] = "O",
	["Ù"] = "U", ["Ú"] = "U", ["Û"] = "U", ["Ü"] = "U",
	["Ý"] = "Y", ["Þ"] = "P", ["ß"] = "s",
	["à"] = "a", ["á"] = "a", ["â"] = "a", ["ã"] = "a",
	["ä"] = "a", ["å"] = "a", ["æ"] = "ae", ["ç"] = "c",
	["è"] = "e", ["é"] = "e", ["ê"] = "e", ["ë"] = "e",
	["ì"] = "i", ["í"] = "i", ["î"] = "i", ["ï"] = "i",
	["ñ"] = "n", ["ð"] = "o", ["œ"] = "oe",
	["ò"] = "o", ["ó"] = "o", ["ô"] = "o",
	["õ"] = "o", ["ö"] = "o", ["ø"] = "o",
	["ù"] = "u", ["ú"] = "u", ["û"] = "u", ["ü"] = "u",
	["ý"] = "y", ["þ"] = "p", ["ÿ"] = "y",
}

function stripAccents(str)
	local normalisedString = ''
	local normalisedString = str:gsub("[%z\1-\127\194-\244][\128-\191]*", accents)
	return normalisedString
end
