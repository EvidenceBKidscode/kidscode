return {
	name = "multi",
	teacher_only = true,
	caption = minetest.colorize("#ff0", fgettext("Créer partie multijoueur")),
	cbf_formspec = function(tabview, name, tabdata)
		local fs = "hypertext[0.2,0.2;8,1;;<big><b>Créer une partie multijoueur</b></big>]" ..
				formspecs.mapselect.get()

		if gamemenu.chosen_map then
			fs = "hypertext[0.2,0.2;14,1;;<big><b>Créer une partie multijoueur - " ..
				core.formspec_escape(gamemenu.chosen_map.name:sub(1,50)) .. "</b></big>]" ..
				"container[0.5,0]" .. formspecs.startmulti.get() .. "container_end[]" ..
				"container[4.5,0]" .. formspecs.mapserver.get() .. "container_end[]" ..
				"container[9.0,0]" .. formspecs.mapinfo.get() .. "container_end[]"
		end

		return fs
	end,

	cbf_button_handler = function(tabview, fields, tabname, tabdata)
		if gamemenu.chosen_map then
			return formspecs.mapinfo.handle(tabview, fields, tabname, tabdata) or
				formspecs.startmulti.handle(tabview, fields, tabname, tabdata) or
				formspecs.mapserver.handle(tabview, fields, tabname, tabdata)	
		end

		return formspecs.mapselect.handle(tabview, fields, tabname, tabdata)
	end,
}
