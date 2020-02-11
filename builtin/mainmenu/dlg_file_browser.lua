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


local PATH = os.getenv("HOME")
local tabdata = {}

local function strip_accents(str)
	local accents = {}

	accents["À"] = "A"
	accents["Ç"] = "C"
	accents["È"] = "E"
	accents["É"] = "E"
	accents["Ê"] = "E"
	accents["Ë"] = "E"
	accents["Ô"] = "O"
	accents["à"] = "a"
	accents["æ"] = "ae"
	accents["ç"] = "c"
	accents["è"] = "e"
	accents["é"] = "e"
	accents["ê"] = "e"
	accents["ë"] = "e"
	accents["î"] = "i"
	accents["ï"] = "i"
	accents["ô"] = "o"
	accents["ö"] = "o"

	return str:gsub("[%z\1-\127\194-\244][\128-\191]*", accents)
end

local function filesize(name)
	local f = io.open(name)
	if not f then return end
	local size = f:seek("end")
	f:close()

	return size
end

local function clean_list(dirs, only_zip)
	local new = {}
	for _, f in ipairs(dirs) do
		if f:sub(1,1) ~= "." then
			local is_file = f:match("^.+(%..+)$")
			if not only_zip or (only_zip and
					(is_file == ".zip" or is_file == ".rar" or not is_file)) then
				new[#new + 1] = f
			end
		end
	end

	table.sort(new)
	return new
end

local function make_fs()
	local dirs = minetest.get_dir_list(PATH)
	dirs = clean_list(dirs, tabdata.show_zip)
	local sel = tabdata.filename or ""

	local _path = strip_accents(PATH)
	_path = _path:gsub("%w+", " <action name=%1>%1</action> ")

	local fs = "size[10,7]" ..
		"real_coordinates[true]" ..
		"image_button[0.2,0.15;0.5,0.5;" ..
			core.formspec_escape(defaulttexturedir .. "arrow_up.png") .. ";updir;]" ..
		"hypertext[1,0.25;9,0.5;path;<style size=20>" .. _path .. "</style>]" ..
		"tablecolumns[image," ..
			"0=" .. core.formspec_escape(defaulttexturedir .. "folder.png") ..
			"," ..
			"1=" .. core.formspec_escape(defaulttexturedir .. "file.png") ..
			";text;text,align=right,padding=1]" ..
		"field[0.2,5.7;6.8,0.5;select;Nom du fichier :;" .. sel .. "]" ..
		"dropdown[7.2,5.7;2.6,0.5;extension;Tous les fichiers,ZIP ou RAR;" ..
			(tabdata.dd_selected or 1) .. "]" ..
		"button[2.8,6.35;2,0.5;ok;Ouvrir]" ..
		"button[5,6.35;2,0.5;cancel;Annuler]"

	local _dirs = ""

	for _, f in ipairs(dirs) do
		local is_file = f:match("^.+(%..+)$")
		_dirs = _dirs .. (is_file and "1," or "0,") .. f .. ","

		if is_file then
			local size = filesize(PATH .. DIR_DELIM .. f) / 1000
			local unit = "KB"

			if size >= 1000000 then
				unit = "MB"
				size = size / 1000
			end

			_dirs = _dirs .. string.format("%.1f", size) .. " " .. unit .. ","
		else
			_dirs = _dirs .. ","
		end
	end

	fs = fs .. "table[0.2,0.8;9.6,4.4;dirs;" .. _dirs:sub(1,-2) ..
		";" .. (tabdata.selected or 1) .. "]"

	return fs
end

local function fields_handler(this, fields)
	print(dump(fields))
	local dirs = minetest.get_dir_list(PATH)
	dirs = clean_list(dirs, tabdata.show_zip)

	if fields.dirs then
		local event, idx = fields.dirs:sub(1,3), tonumber(fields.dirs:match("%d+"))
		local filename = dirs[idx]

		if event == "CHG" then
			local is_file = filename:find("^.+(%..+)$")
			tabdata.filename = is_file and filename or ""
			tabdata.selected = idx

			core.update_formspec(this:get_formspec())
			return true

		elseif event == "DCL" then
			local is_file = filename:find("^.+(%..+)$")
			if not is_file then
				PATH = PATH .. (PATH == DIR_DELIM and "" or DIR_DELIM) .. filename
				tabdata.selected = 1
			end

			core.update_formspec(this:get_formspec())
			return true
		end
	elseif fields.updir then
		PATH = string.split(PATH, DIR_DELIM)
		PATH[#PATH] = nil
		PATH = table.concat(PATH, DIR_DELIM)
		PATH = DIR_DELIM .. PATH

		tabdata.selected = 1

		core.update_formspec(this:get_formspec())
		return true

	elseif fields.path then
		local dir = fields.path:match(":(%w+)$")

		PATH = string.split(PATH, DIR_DELIM)
		local newpath = DIR_DELIM

		for _, v in ipairs(PATH) do
			newpath = newpath .. v .. DIR_DELIM
			if v == dir then break end
		end

		PATH = newpath:sub(1,-2)

		core.update_formspec(this:get_formspec())
		return true

	elseif fields.extension == "Tous les fichiers" then
		tabdata.show_zip = nil
		tabdata.dd_selected = 1

		core.update_formspec(this:get_formspec())
		return true

	elseif fields.extension == "ZIP ou RAR" then
		tabdata.show_zip = true
		tabdata.dd_selected = 2

		core.update_formspec(this:get_formspec())
		return true
	end

	return false
end

function create_file_browser_dlg()
	return dialog_create("settings_advanced", make_fs, fields_handler)
end
