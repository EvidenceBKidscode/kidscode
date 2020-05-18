local function get_fs()
	return "hypertext[0.2,0.2;8,1;;<big>Tutoriel</big>]"
end

return {
	name = "help",
	caption = fgettext("Tutoriel"),
	cbf_formspec = get_fs,
	cbf_button_handler = function()
		return true
	end,
}
