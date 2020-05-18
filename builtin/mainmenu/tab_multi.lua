return {
	name = "multi",
	caption = minetest.colorize("#ff0", "    " .. fgettext("Créer partie multijoueur")),
	cbf_formspec = gamedata.get_formspec_solo,
	cbf_button_handler = gamedata.main_button_handler_solo,
}
