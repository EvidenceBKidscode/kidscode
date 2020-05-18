function get_formspec_slideshow(tabview, name, tabdata)
	return "animated_image[0,0;14,9.5;;" ..
		core.formspec_escape(defaulttexturedir .. "slideshow.png") .. ";3;3000]"
end

return {
	name = "slideshow",
	cbf_formspec = get_formspec_slideshow,
}
