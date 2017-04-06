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

local core_developers = {
	"Perttu Ahola (celeron55) <celeron55@gmail.com>",
	"Ryan Kwolek (kwolekr) <kwolekr@minetest.net>",
	"PilzAdam <pilzadam@minetest.net>",
	"sfan5 <sfan5@live.de>",
	"kahrl <kahrl@gmx.net>",
	"sapier",
	"ShadowNinja <shadowninja@minetest.net>",
	"Nathanaël Courant (Nore/Ekdohibs) <nore@mesecons.net>",
	"Loic Blot (nerzhul/nrz) <loic.blot@unix-experience.fr>",
	"Matt Gregory (paramat)",
	"est31 <MTest31@outlook.com>",
	"Craig Robbins (Zeno) <craig.d.robbins@gmail.com>",
	"Auke Kok (sofar) <sofar@foo-projects.org>",
	"Andrew Ward (rubenwardy) <rubenwardy@gmail.com>",
}

local active_contributors = {
	"Duane Robertson <duane@duanerobertson.com>",
	"SmallJoker <mk939@ymail.com>",
	"Lars Hofhansl <larsh@apache.org>",
	"Jeija <jeija@mesecons.net>",
	"Gregory Currie (gregorycu)",
	"Sokomine <wegwerf@anarres.dyndns.org>",
	"TeTpaAka",
	"Jean-Patrick G (kilbith) <jeanpatrick.guerrero@gmail.com>",
	"Diego Martínez (kaeza) <kaeza@users.sf.net>",
	"Dániel Juhász (juhdanad) <juhdanad@gmail.com>",
	"Rogier <rogier777@gmail.com>",
}

local function get_formspec()
	local logofile = defaulttexturedir .. "logo.png"
	local version = core.get_version()
	return
		"size[12,5.5;true]" ..
		"background[-0.4,-0.6;12.8,7;" .. defaulttexturedir .. "mainmenu_bg_solo.png]" ..
		"image_button[0,5;2,0.8;" .. defaulttexturedir .. "mainmenu_button.png;btn_back;" ..
			minetest.colorize("#333333", fgettext("< Back")) .. ";;false]" ..
		"tablecolumns[color;text]" ..
		"tableoptions[background=#00000000;highlight=#00000000;color=#333333;border=false]" ..
		"table[3.5,-0.25;8.5,6.05;list_credits;" ..
		"#b57614," .. fgettext("Minetest Core Developers") .. ",," ..
		table.concat(core_developers, ",,") .. ",,," ..
		"#b57614," .. fgettext("Minetest Active Contributors") .. ",," ..
		table.concat(active_contributors, ",,") .. ",,," ..
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
