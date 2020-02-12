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


local function get_formspec(tabview, name, tabdata)
	local retval = ""

	local index = filterlist.get_current_index(menudata.worldlist,
			tonumber(core.settings:get("mainmenu_last_selected_world")))

	local worldlist = menu_render_worldlist()

	if worldlist ~= "" then
		retval = retval ..
			"button[9,4;2.3,0.6;world_delete;".. fgettext("Supprimer") .. "]"
	end

	retval = retval ..
		"label[4,-0.25;".. fgettext("Sélectionner un monde :") .. "]"..

		"tooltip[0.25,0.5;2,0.2;" ..
			core.wrap_text("Si vous êtes enseignant, cochez cette case " ..
				"pour que votre ordinateur fasse office de serveur. " ..
				"Vos élèves pourront ensuite rejoindre votre partie depuis " ..
				"le menu 'Rejoindre une partie', en indiquant l'adresse IP " ..
				"de votre poste et le port (30000 par défaut).", 80) ..
		"]" ..
		"checkbox[0.25,0.5;advanced_options;" .. fgettext("Options avancées") .. ";" ..
			dump(core.settings:get_bool("advanced_options")) .. "]" ..
		"checkbox[0.25,1;cb_server;".. fgettext("Héberger un serveur") ..";" ..
			dump(core.settings:get_bool("enable_server")) .. "]" ..
		"textlist[4,0.25;7.7,3.7;sp_worlds;" .. worldlist .. ";" .. index .. "]" ..

		"tooltip[4,4.7;2.2,0.6;" ..
			core.wrap_text("Cliquez ici pour ajouter une carte téléchargée " ..
				"depuis le site de l'IGN (formats ZIP et RAR acceptés)", 80) ..
		"]" ..
		"button[6.7,4;2.2,0.6;world_import;".. fgettext("Importer") .. "]" ..
		"button[4.4,4;2.2,0.6;play;".. fgettext("Jouer") .. ";#0000ff]"

	if core.settings:get_bool("advanced_options") then
		retval = retval ..
			"button[5.5,4.7;2.2,0.6;world_configure;".. fgettext("Configurer") .. "]" ..
			"button[7.8,4.7;2.2,0.6;world_create;".. fgettext("Nouveau") .. "]"
	end

	if core.settings:get_bool("enable_server") then
		retval = retval ..
			"label[0.25,1.7;" .. fgettext("Nom / Pseudonyme") .. "]" ..
			"field[0.25,1.9;3.5,0.5;te_playername;;" ..
				core.formspec_escape(core.settings:get("name")) .. "]" ..
			"label[0.25,2.7;" .. fgettext("Mot de passe (optionnel)") .. "]" ..
			"pwdfield[0.25,2.9;3.5,0.5;te_passwd;]"

		local bind_addr = core.settings:get("bind_address")
		if bind_addr ~= nil and bind_addr ~= "" then
			retval = retval ..
				"field[0.55,5.2;2.25,0.5;te_serveraddr;" .. fgettext("Bind Address") .. ";" ..
					core.formspec_escape(core.settings:get("bind_address")) .. "]" ..
				"field[2.8,5.2;1.25,0.5;te_serverport;" .. fgettext("Port") .. ";" ..
					core.formspec_escape(core.settings:get("port")) .. "]"
		else
			retval = retval ..
				"field[0.25,3.85;3.5,0.5;te_serverport;" .. fgettext("Port du serveur") .. ";" ..
				core.formspec_escape(core.settings:get("port")) .. "]"
		end	
	end

	return retval
end

local function main_button_handler(this, fields, name, tabdata)

	assert(name == "local")

	local world_doubleclick = false

	if fields["sp_worlds"] ~= nil then
		local event = core.explode_textlist_event(fields["sp_worlds"])
		local selected = core.get_textlist_index("sp_worlds")

		menu_worldmt_legacy(selected)

		if event.type == "DCL" then
			world_doubleclick = true
		end

		if event.type == "CHG" and selected ~= nil then
			core.settings:set("mainmenu_last_selected_world",
				menudata.worldlist:get_raw_index(selected))
			return true
		end
	end

	if menu_handle_key_up_down(fields,"sp_worlds","mainmenu_last_selected_world") then
		return true
	end

	if fields["cb_creative_mode"] then
		core.settings:set("creative_mode", fields["cb_creative_mode"])
		local selected = core.get_textlist_index("sp_worlds")
		menu_worldmt(selected, "creative_mode", fields["cb_creative_mode"])

		return true
	end

	if fields["cb_enable_damage"] then
		core.settings:set("enable_damage", fields["cb_enable_damage"])
		local selected = core.get_textlist_index("sp_worlds")
		menu_worldmt(selected, "enable_damage", fields["cb_enable_damage"])

		return true
	end

	if fields["cb_server"] then
		core.settings:set("enable_server", fields["cb_server"])

		return true
	end

	if fields["cb_server_announce"] then
		core.settings:set("server_announce", fields["cb_server_announce"])
		local selected = core.get_textlist_index("srv_worlds")
		menu_worldmt(selected, "server_announce", fields["cb_server_announce"])

		return true
	end

	if fields["play"] ~= nil or world_doubleclick or fields["key_enter"] then
		local selected = core.get_textlist_index("sp_worlds")
		gamedata.selected_world = menudata.worldlist:get_raw_index(selected)

		if core.settings:get_bool("enable_server") then
			if selected ~= nil and gamedata.selected_world ~= 0 then
				gamedata.playername = fields["te_playername"]
				gamedata.password   = fields["te_passwd"]
				gamedata.port       = fields["te_serverport"]
				gamedata.address    = ""

				core.settings:set("port",gamedata.port)
				if fields["te_serveraddr"] ~= nil then
					core.settings:set("bind_address",fields["te_serveraddr"])
				end

				core.start()
			else
				gamedata.errormessage =
					fgettext("No world created or selected!")
			end
		else
			if selected ~= nil and gamedata.selected_world ~= 0 then
				gamedata.singleplayer = true
				core.start()
			else
				gamedata.errormessage =
					fgettext("No world created or selected!")
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

	if fields["world_create"] ~= nil then
		local create_world_dlg = create_create_world_dlg(true)
		create_world_dlg:set_parent(this)
		this:hide()
		create_world_dlg:show()
		return true
	end

	if fields["world_delete"] ~= nil then
		local selected = core.get_textlist_index("sp_worlds")
		if selected ~= nil and
			selected <= menudata.worldlist:size() then
			local world = menudata.worldlist:get_list()[selected]
			if world ~= nil and
				world.name ~= nil and
				world.name ~= "" then
				local index = menudata.worldlist:get_raw_index(selected)
				local delete_world_dlg = create_delete_world_dlg(world.name,index)
				delete_world_dlg:set_parent(this)
				this:hide()
				delete_world_dlg:show()
			end
		end

		return true
	end

	if fields["world_configure"] ~= nil then
		local selected = core.get_textlist_index("sp_worlds")
		if selected ~= nil then
			local configdialog =
				create_configure_world_dlg(
					menudata.worldlist:get_raw_index(selected))

			if (configdialog ~= nil) then
				configdialog:set_parent(this)
				this:hide()
				configdialog:show()
			end
		end

		return true
	end

	if fields["world_import"] then
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
