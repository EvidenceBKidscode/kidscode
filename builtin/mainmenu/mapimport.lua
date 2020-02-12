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

local function install_map(parent, tempdir, mapname)
	local mappath = core.get_worldpath() .. DIR_DELIM .. mapname

	if dir_exists(mappath) then
		show_question(parent,
			("Une carte %s existe déja. Choisissez un autre nom :"):
			format(core.colorize("#EE0", mapname)), mapname,
			function(this, fields)
				return install_map(this, tempdir, fields.dlg_mapimport_formspec_value)
			end,
			function(this)
				core.delete_dir(tempdir)
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

local function import_map(parent, zippath)
	local zipname = string.match(zippath, "([^%/]*)$")

	-- Create a temp directory for decompressing zip file in it
	local tempdir = os.tempfolder() .. DIR_DELIM .. "TEST";
	core.create_dir(tempdir)

	-- Extract archive
	if not core.extract_zip(zippath, tempdir) then
		core.log("warning", "Unable to extract zipfile " .. zippath .. " to " .. tempdir)
		show_message(parent,
			("Impossible d'ouvrir l'archive %s."):
			format(core.colorize("#EE0", zipname)))
		return true
	end

	-- Check content
	local files = core.get_dir_list(tempdir, true)
	if (files == nil) then
		core.log("warning", "No file found in "..zippath)
		show_message(parent,
			("Pas de dossier trouvé dans l'archive %s."):
			format(core.colorize("#EE0", zipname)))
		return true
	end

	if #files ~= 1 then
		core.log("warning", "Too many files in " .. zippath)
		show_message(parent,
			("L'archive %s doit contenir un dossier seul."):
			format(core.colorize("#EE0", zipname)))
		return true
	end

	-- Install map (ie copy unique folder to world dir)
	return install_map(parent, tempdir .. DIR_DELIM .. files[1], files[1])
end

return import_map
