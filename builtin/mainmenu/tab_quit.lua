return {
	name = "quit",
	icon = "quit.png",
	caption = fgettext("Quitter"),
	cbf_formspec = function()
		os.exit(0)
		return ""
	end,
	cbf_button_handler = function()
		return true
	end,
}
