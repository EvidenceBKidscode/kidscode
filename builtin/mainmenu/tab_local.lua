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

dofile(core.get_mainmenu_path() .. DIR_DELIM .. "maputils.lua")
local btn_status = "installed"

local btn_msg = {
	installed = {"Jouer", "play"},
	ready     = {"Installer", "install"},
	cancelled = {"Réinstaller", "reinstall"},
}

local function get_formspec(tabview, name, tabdata)
	local retval = ""

	local index = filterlist.get_current_index(menudata.worldlist,
			tonumber(core.settings:get("mainmenu_last_selected_world")))

	local worldlist = menu_render_worldlist()

	if worldlist ~= "" then
		retval = retval ..
			"button[9.4,4;2.3,0.6;world_delete;".. fgettext("Supprimer") .. "]"
	end

	retval = retval ..
		"tooltip[0.25,1;2,0.2;" ..
			core.wrap_text("Cochez cette case pour que votre poste fasse office de serveur local. " ..
				"Indiquez votre pseudonyme et laissez le port tel quel. " ..
				"Sélectionnez une carte et lancez le serveur. " ..
				"Vos élèves peuvent ensuite rejoindre votre monde depuis le menu \"Rejoindre un serveur\". " ..
				"Si votre serveur est en cours d'éxécution, il sera visible par tous les élèves.", 80) .. "]" ..
		"checkbox[0.25,0.5;advanced_options;" .. fgettext("Options avancées") .. ";" ..
			dump(core.settings:get_bool("advanced_options")) .. "]" ..
		"checkbox[0.25,1;cb_server;" .. fgettext("Héberger un serveur") .. ";" ..
			dump(core.settings:get_bool("enable_server")) .. "]" ..
		"tooltip[6.7,4;2.2,0.6;" ..
			core.wrap_text("Cliquez ici pour ajouter une carte téléchargée " ..
				"depuis le site de l'IGN (formats ZIP et RAR acceptés)", 80) .. "]" ..
		"button[7.1,4;2.2,0.6;world_import;" .. fgettext("Importer") .. "]" ..
		"tablecolumns[color;text;color;text,padding=1;color;text,align=center,padding=1;" ..
			     "color;text,align=center,padding=1]"

	if btn_msg[btn_status] then
		retval = retval ..
			"button[3.5,4;2.2,0.6;" .. btn_msg[btn_status][2] .. ";" ..
				btn_msg[btn_status][1] .. ";#0000ff]"
	end

	local wl = "#ff00ff,Carte,#ff00ff,Demande,#ff00ff,Origine,#ff00ff,Etat"
	wl = wl .. "," .. worldlist

	retval = retval ..
		"table[3.5,0.25;8.2,3.7;sp_worlds;" .. wl .. ";" .. index .. "]"

	if core.settings:get_bool("advanced_options") then
		retval = retval ..
			"button[7.1,4.7;2.2,0.6;world_configure;" .. fgettext("Configurer") .. "]" ..
			"button[9.4,4.7;2.3,0.6;world_create;" .. fgettext("Nouveau") .. "]"
	end

	if core.settings:get_bool("enable_server") then
		retval = retval ..
			"label[0.25,1.7;" .. fgettext("Nom / Pseudonyme") .. "]" ..
			"field[0.25,1.9;3,0.5;te_playername;;" ..
				core.formspec_escape(core.settings:get("name")) .. "]" ..
			"label[0.25,2.7;" .. fgettext("Mot de passe (optionnel)") .. "]" ..
			"pwdfield[0.25,2.9;3,0.5;te_passwd;]"

		local bind_addr = core.settings:get("bind_address")
		if bind_addr and bind_addr ~= "" then
			retval = retval ..
				"field[0.55,5.2;2.25,0.5;te_serveraddr;" .. fgettext("Bind Address") .. ";" ..
					core.formspec_escape(core.settings:get("bind_address")) .. "]" ..
				"field[2.8,5.2;1.25,0.5;te_serverport;" .. fgettext("Port") .. ";" ..
					core.formspec_escape(core.settings:get("port")) .. "]"
		else
			retval = retval ..
				"field[0.25,3.85;3,0.5;te_serverport;" .. fgettext("Port du serveur") .. ";" ..
				core.formspec_escape(core.settings:get("port")) .. "]"
		end
	end

	return retval
end

local function main_button_handler(this, fields, name, tabdata)
	assert(name == "local")
--	print(dump(fields))

	local world_doubleclick = false

	if fields.sp_worlds then
		local event, selected = fields.sp_worlds:match("(.*):(%d+):")
		selected = math.max(1, tonumber(selected) - 1)

		menu_worldmt_legacy(selected)

		if event == "DCL" then
			world_doubleclick = true
		end

		if event == "CHG" and selected then
			local wl = menudata.worldlist
			core.settings:set("mainmenu_last_selected_world", wl:get_raw_index(selected + 1))

			local world = wl:get_list()[selected]
			btn_status = world.status

			return true
		end
	end

	if menu_handle_key_up_down(fields, "sp_worlds", "mainmenu_last_selected_world") then
		return true
	end

	if fields.cb_enable_damage then
		core.settings:set("enable_damage", fields.cb_enable_damage)
		local selected = core.get_textlist_index("sp_worlds")
		menu_worldmt(selected, "enable_damage", fields.cb_enable_damage)

		return true
	end

	if fields.cb_server then
		core.settings:set("enable_server", fields.cb_server)
		return true
	end

	if fields.install then
		local idx = core.settings:get("mainmenu_last_selected_world")
		local map = menudata.worldlist:get_raw_element(idx)
		mapmgr.install_map_from_web(this, map)

		mapmgr.show_message(this,
			"Veuillez patienter pendant le téléchargement et l'installation de la carte.")
		return true
	end

	if fields.play or world_doubleclick or fields.key_enter then
		local selected = core.get_textlist_index("sp_worlds")
		gamedata.selected_world = menudata.worldlist:get_raw_index(selected)

		if core.settings:get_bool("enable_server") then
			if selected and gamedata.selected_world ~= 0 then
				gamedata.playername = fields.te_playername
				gamedata.password   = fields.te_passwd
				gamedata.port       = fields.te_serverport
				gamedata.address    = ""

				core.settings:set("port", gamedata.port)
				if fields.te_serveraddr then
					core.settings:set("bind_address", fields.te_serveraddr)
				end

				core.start()
			else
				gamedata.errormessage = fgettext("No world created or selected!")
			end
		else
			if selected and gamedata.selected_world ~= 0 then
				gamedata.singleplayer = true
				core.start()
			else
				gamedata.errormessage = fgettext("No world created or selected!")
			end

			return true
		end
	end

	if fields.advanced_options then
		if core.settings:get_bool("advanced_options") then
			core.settings:set("advanced_options", "")
		else
			core.settings:set("advanced_options", "true")
		end

		return true
	end

	if fields.world_create then
		local create_world_dlg = create_create_world_dlg(true)
		create_world_dlg:set_parent(this)
		this:hide()
		create_world_dlg:show()

		return true
	end

	if fields.world_delete then
		local selected = core.get_textlist_index("sp_worlds")
		if selected and selected <= menudata.worldlist:size() then
			local world = menudata.worldlist:get_list()[selected]
			if world and world.name and world.name ~= "" then
				local index = menudata.worldlist:get_raw_index(selected)
				local delete_world_dlg = create_delete_world_dlg(world.name,index)
				delete_world_dlg:set_parent(this)
				this:hide()
				delete_world_dlg:show()
			end
		end

		return true
	end

	if fields.world_configure then
		local selected = core.get_textlist_index("sp_worlds")
		if selected then
			local configdialog = create_configure_world_dlg(
				menudata.worldlist:get_raw_index(selected))

			if configdialog then
				configdialog:set_parent(this)
				this:hide()
				configdialog:show()
			end
		end

		return true
	end

	if fields.world_import then
		local file_browser_dlg = create_file_browser_dlg()
		file_browser_dlg:set_parent(this)
		this:hide()
		file_browser_dlg:show()

		return true
	end
end
--------------------------------------------------------------------------------
return {
	name = "local",
	caption = fgettext("Lancer une session"),
	cbf_formspec = get_formspec,
	cbf_button_handler = main_button_handler,
}
