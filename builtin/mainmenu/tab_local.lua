return {
	name = "local",
	caption = fgettext("Modes de jeu"),
	cbf_formspec = get_formspec_slideshow,
	cbf_button_handler = function()
		return true
	end,
}
