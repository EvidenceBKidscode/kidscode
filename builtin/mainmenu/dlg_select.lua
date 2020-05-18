local ESC = core.formspec_escape

local function get_formspec()
	local play_mode = gamedata.play_mode
	local title = play_mode == "solo" and "Partie solo" or "Créer une partie multijoueur"
	title = title .. " - " .. gamedata.selected_world.name

	local fs = "size[10,8]" ..
		"hypertext[0.2,0;8,1;;<big>" .. title .. "</big>]" ..

		"style[play;border=false;bgimg_hovered=" .. ESC(defaulttexturedir .. "select.png") .. "]" ..
		"image_button[0.5,1.2;2.5,2.5;" .. ESC(defaulttexturedir .. "img_" .. play_mode .. ".png") .. ";;]" ..
		"image_button[0.3,1;2.9,3.3;" .. ESC(defaulttexturedir .. "blank.png") .. ";play;]" ..
		"label[1.05,3.6;Jouer en 3D]" ..

		"style[carto;border=false;bgimg_hovered=" .. ESC(defaulttexturedir .. "select.png") .. "]" ..
		"image_button[3.9,1.2;2.5,2.5;" .. ESC(defaulttexturedir .. "img_carto.png") .. ";;]" ..
		"image_button[3.7,1;2.9,3.3;" .. ESC(defaulttexturedir .. "blank.png") .. ";carto;]" ..
		"label[4.15,3.6;Cartographie en 2D]"

	if play_mode == "multi" then
		fs = fs .. "checkbox[0.6,4;cb_advanced;Options avancées;" ..
			(dump(core.settings:get_bool("cb_advanced") or false)) .. "]"

		if core.settings:get_bool("cb_advanced") then
			fs = fs .. "container[0.3,3.7]" ..
				"field[0.25,1.9;3,0.5;te_playername;Nom / Pseudonyme;" ..
					ESC(core.settings:get("name")) .. "]" ..
				"pwdfield[0.25,3;3,0.5;te_passwd;Mot de passe (optionnel)]" ..
				"field[0.25,4.1;3,0.5;te_serverport;Port du serveur;" ..
					ESC(core.settings:get("port")) .. "]" ..
				"container_end[]"
		end
	end

	return fs
end

local function fields_handler(this, fields)
	if fields.cb_advanced ~= nil then
		core.settings:set_bool("cb_advanced", fields.cb_advanced)
		return true
	end

	if fields.play then
		gamedata.singleplayer = gamedata.play_mode == "solo"
		core.start()
		return true
	end

	return false
end

function create_select_map_dlg(current_tab)
	return dialog_create("select_map", get_formspec, fields_handler)
end
