--Minetest
--Copyright (C) 2014 sapier
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

mt_color_grey  = "#AAAAAA"
mt_color_blue  = "#6389FF"
mt_color_green = "#72FF63"
mt_color_dark_green = "#25C191"

--for all other colors ask sfan5 to complete his work!

local menupath = core.get_mainmenu_path()
local basepath = core.get_builtin_path()
local menustyle = core.settings:get("main_menu_style")

defaulttexturedir = core.get_texturepath_share() .. DIR_DELIM ..
			"base" .. DIR_DELIM .. "pack" .. DIR_DELIM

dofile(basepath .. "common" .. DIR_DELIM .. "async_event.lua")
dofile(basepath .. "common" .. DIR_DELIM .. "filterlist.lua")
dofile(basepath .. "fstk" .. DIR_DELIM .. "buttonbar.lua")
dofile(basepath .. "fstk" .. DIR_DELIM .. "dialog.lua")
dofile(basepath .. "fstk" .. DIR_DELIM .. "tabview.lua")
dofile(basepath .. "fstk" .. DIR_DELIM .. "tabview_layouts.lua")
dofile(basepath .. "fstk" .. DIR_DELIM .. "ui.lua")

dofile(menupath .. DIR_DELIM .. "mapmgr.lua")
dofile(menupath .. DIR_DELIM .. "common.lua")
dofile(menupath .. DIR_DELIM .. "pkgmgr.lua")
dofile(menupath .. DIR_DELIM .. "formspecs.lua")
dofile(menupath .. DIR_DELIM .. "gamemenu.lua")

dofile(menupath .. DIR_DELIM .. "dlg_config_world.lua")
dofile(menupath .. DIR_DELIM .. "dlg_file_browser.lua")
dofile(menupath .. DIR_DELIM .. "dlg_select.lua")
dofile(menupath .. DIR_DELIM .. "dlg_settings_advanced.lua")
--dofile(menupath .. DIR_DELIM .. "dlg_contentstore.lua")
dofile(menupath .. DIR_DELIM .. "dlg_change_game.lua")

if menustyle ~= "simple" then
	dofile(menupath .. DIR_DELIM .. "dlg_create_world.lua")
	dofile(menupath .. DIR_DELIM .. "dlg_delete_content.lua")
	dofile(menupath .. DIR_DELIM .. "dlg_delete_world.lua")
	dofile(menupath .. DIR_DELIM .. "dlg_rename_modpack.lua")
end

local tabs = {}
--tabs.content  = dofile(menupath .. DIR_DELIM .. "tab_content.lua")
tabs.solo = dofile(menupath .. DIR_DELIM .. "tab_solo.lua")
tabs.multi = dofile(menupath .. DIR_DELIM .. "tab_multi.lua")
tabs.online = dofile(menupath .. DIR_DELIM .. "tab_online.lua")
tabs.settings  = dofile(menupath .. DIR_DELIM .. "tab_settings.lua")
tabs.slideshow = dofile(menupath .. DIR_DELIM .. "tab_slideshow.lua")
tabs.help      = dofile(menupath .. DIR_DELIM .. "tab_help.lua")
tabs.quit      = dofile(menupath .. DIR_DELIM .. "tab_quit.lua")

if menustyle == "simple" then
	tabs.simple_main = dofile(menupath .. DIR_DELIM .. "tab_simple_main.lua")
else
	tabs.local_game = dofile(menupath .. DIR_DELIM .. "tab_local.lua")
end

--------------------------------------------------------------------------------
local function main_event_handler(tabview, event)
	if event == "MenuQuit" then
		core.close()
	end
	return true
end

--------------------------------------------------------------------------------
function create_worldlist()
	menudata.worldlist = filterlist.create(
--			core.get_worlds,
		mapmgr.preparemaplist,
--			compare_worlds,
		mapmgr.compare_map,
		-- Unique id comparison function
		function(element, uid)
			return element.uid == uid
		end,
		-- Filter function
		function(element, gameid)
			return element.gameid == gameid
		end,
		nil
	)

	menudata.worldlist:add_sort_mechanism("name", sort_worlds_alphabetic)
	menudata.worldlist:add_sort_mechanism("demand", sort_worlds_by_demand)
	menudata.worldlist:add_sort_mechanism("origin", sort_worlds_by_origin)
	menudata.worldlist:add_sort_mechanism("status", sort_worlds_by_status)
	menudata.worldlist:set_sortmode("status")
end

--------------------------------------------------------------------------------
local function init_globals()
	-- Init gamedata
	gamedata.worldindex = 0

	if menustyle == "simple" then
		local world_list = core.get_worlds()
		local world_index
		local found_singleplayerworld = false

		for i, world in ipairs(world_list) do
			if world.name == "singleplayerworld" then
				found_singleplayerworld = true
				world_index = i
				break
			end
		end

		if not found_singleplayerworld then
			core.create_world("singleplayerworld", 1)

			world_list = core.get_worlds()

			for i, world in ipairs(world_list) do
				if world.name == "singleplayerworld" then
					world_index = i
					break
				end
			end
		end

		gamedata.worldindex = world_index
	else
		create_worldlist()

		if not core.settings:get("menu_current_game") then
			local default_game = core.settings:get("default_game") or "minetest"
			core.settings:set("menu_current_game", default_game)
		end

		gamemenu.init()
	end

	-- Create main tabview
	local tv_main = tabview_create("maintab", {x = 12, y = 5.5}, {x = 0, y = 0}, tabview_layouts.vertical)
	if menustyle == "simple" then
		tv_main:add(tabs.simple_main)
	else
		tv_main:set_autosave_tab(false)
		tv_main:add(tabs.local_game)
	end

	tv_main:add(tabs.solo)
	tv_main:add(tabs.multi)
	tv_main:add(tabs.online)

	--tv_main:add(tabs.content)
	tv_main:add(tabs.help)
	tv_main:add(tabs.settings)
	tv_main:add(tabs.quit)
	tv_main:add(tabs.slideshow)

	tv_main:set_global_event_handler(main_event_handler)
	tv_main:set_fixed_size(false)

	if menustyle ~= "simple" then
		--[[
		local last_tab = core.settings:get("maintab_LAST")
		if last_tab and tv_main.current_tab ~= last_tab then
			tv_main:set_tab(last_tab)
		end
		]]

		tv_main:set_tab("slideshow")
	end

	ui.set_default("maintab")

	-- >> KIDSCODE - get back to same menu page after playing
	local chosen = tonumber(core.volatile_settings:get("mainmenu_last_chosen_world"))
	if chosen then
		core.volatile_settings:remove("mainmenu_last_chosen_world")
		gamemenu.chosen_map = menudata.worldlist:get_raw_element(chosen)
	end

	local tab = core.volatile_settings:get("mainmenu_last_tab")
	if tab then
		core.volatile_settings:remove("mainmenu_last_tab")
		tv_main:set_tab(tab)
	end
	-- << KIDSCODE - get back to same menu page after playing

	tv_main:show()

	ui.update()

	core.sound_play("main_menu", true)
end

init_globals()

core.mapserver_event_handler = function(ev)
	core.event_handler("Refresh")
end
