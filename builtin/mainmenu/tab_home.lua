local function get_formspec(tabview, name, tabdata)
	local retval = ""

	retval = retval ..
		"bgcolor[#080808BB;true]" ..
		"image_button[3.25,0;5.4,1.4;" ..
			core.formspec_escape(defaulttexturedir .. "singleplayer.png") .. ";solo;;true;false]" ..
		"image_button[3.25,1.5;5.4,1.4;" ..
			core.formspec_escape(defaulttexturedir .. "multiplayer.png") .. ";multi;;true;false]" ..
		"image_button[3.25,3;5.4,1.4;" ..
			core.formspec_escape(defaulttexturedir .. "settings.png") .. ";settings;;true;false]" ..
		"image_button[3.25,4.5;5.4,1.4;" ..
			core.formspec_escape(defaulttexturedir .. "credits.png") .. ";credits;;true;false]"


	return retval
end

local function main_button_handler(this, fields, name, tabdata)
	mm_texture.update("home", nil)

	if fields.solo then
		local solo = create_solo_dlg()
		solo:set_parent(this)
		this:hide()
		solo:show()
		return true
	elseif fields.multi then
		asyncOnlineFavourites()
		local multi = create_multi_dlg()
		multi:set_parent(this)
		this:hide()
		multi:show()
		return true
	elseif fields.settings then
		local settings = create_settings_dlg()
		settings:set_parent(this)
		this:hide()
		settings:show()
		return true
	elseif fields.credits then
		local credits = create_credits_dlg()
		credits:set_parent(this)
		this:hide()
		credits:show()
		return true
	end
end

local function on_change(type, old_tab, new_tab)
	mm_texture.update("home", nil)
end

--------------------------------------------------------------------------------
return {
	name = "home",
	caption = fgettext("Home"),
	cbf_formspec = get_formspec,
	cbf_button_handler = main_button_handler,
	on_change = on_change
}
