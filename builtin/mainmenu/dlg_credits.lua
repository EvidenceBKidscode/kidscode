--Minetest
--Copyright (C) 2013 sapier
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

--------------------------------------------------------------------------------

local credits = {
	"© 2016-2017 Kidscode • developed upon the Minetest engine",
	"",
	"Didier Plasse • serial startuper",
	"Jean-Patrick Guerrero • heroic developer",
	"Andrei Toykhov • magic romanian educator",
}

local function get_formspec()
	local logofile = defaulttexturedir .. "logo.png"
	local version = core.get_version()
	return
		"size[12,5.5;true]" ..
		"image[0.5,1;" .. core.formspec_escape(logofile) .. "]" ..
		"label[0.5,3.2;" .. core.colorize("#333333", version.project .. " " .. version.string) .. "]" ..
		"background[-0.4,-0.6;12.8,7;" .. defaulttexturedir .. "mainmenu_bg_solo.png]" ..
		"image_button[0,5;2,0.8;" .. defaulttexturedir .. "mainmenu_button.png;btn_back;" ..
			minetest.colorize("#333333", fgettext("< Back")) .. ";;false]" ..
		"tablecolumns[color;text]" ..
		"tableoptions[background=#00000000;highlight=#00000000;color=#333333;border=false]" ..
		"table[3.5,-0.25;8.5,6.05;list_credits;" ..
		"#b57614," .. fgettext("The Kidscode's Crew") .. ",," ..
		table.concat(credits, ",,") .. ",,," ..
		";1]"
end

local function main_button_handler(this, fields)
	if fields.btn_back then
		this:delete()
	end

	return true
end

function create_credits_dlg()
	local dlg = dialog_create("credits",
				  get_formspec,
				  main_button_handler,
				  nil)
	return dlg
end
