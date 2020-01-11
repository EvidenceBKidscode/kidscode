return {
	name = "quit",
	caption = fgettext("Quit"),
	cbf_formspec = function()
		os.exit(0)
		return ""
	end,
	cbf_button_handler = function()
		return true
	end,
}
