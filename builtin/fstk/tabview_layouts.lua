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


local ESC = core.formspec_escape

tabview_layouts = {}

tabview_layouts.tabs = {
	get = function(self, view)
		local formspec = ""
		local tsize = view.tablist[view.last_tab_index].tabsize or
				{width=view.width, height=view.height}
		if view.parent == nil then
			formspec = formspec ..
					string.format("size[%f,%f,%s]",tsize.width,tsize.height,
							dump(view.fixed_size))
		end
		formspec = formspec .. self:get_header(view)
		formspec = formspec ..
				view.tablist[view.last_tab_index].get_formspec(
						view,
						view.tablist[view.last_tab_index].name,
						view.tablist[view.last_tab_index].tabdata,
						view.tablist[view.last_tab_index].tabsize
				)

		formspec = formspec .. ([[
			style[change_game;noclip=true]
			button[%f,%f;4,0.8;change_game;%s]
		]]):format(-0.3, tsize.height + 0.8, fgettext("Change Game"))

		return formspec
	end,

	get_header = function(self, view)
		local toadd = ""
		for i=1,#view.tablist do
			if toadd ~= "" then
				toadd = toadd .. ","
			end
			toadd = toadd .. view.tablist[i].caption
		end

		return string.format("tabheader[%f,%f;%s;%s;%i;true;false]",
				view.header_x, view.header_y, view.name, toadd, view.last_tab_index);
	end,

	handle_buttons = function(self, view, fields)
		if fields.change_game then
			local change_game_dlg = change_game_dlg()
			change_game_dlg:set_parent(view)
			view:hide()
			change_game_dlg:show()
			return true
		end

		--save tab selection to config file
		if fields[view.name] then
			local index = tonumber(fields[view.name])
			view:switch_to_tab(index)
			return true
		end

		return false
	end,
}


tabview_layouts.vertical = {
	bgcolor = "#ACACAC33",
	selcolor = "#ACACAC66",
	mainbgcolor = "#3c3836ee",

	get = function(self, view)
		local formspec = ""

		local tab = view.tablist[view.last_tab_index]

		if view.parent == nil then
			local tsize = tab and tab.tabsize or {width = view.width, height = view.height}
			formspec = formspec ..
				string.format("formspec_version[3]size[%f,%f,%s]bgcolor[#00000000]",
					tsize.width + 7.5, tsize.height + 4, dump(view.fixed_size))
		end
		formspec = formspec .. self:get_header(view)

		if tab then
			formspec = formspec .. "container[5.5,0]"

			local mainbgcolor = view.mainbgcolor or self.mainbgcolor

			formspec = formspec .. ("box[0,0;%f,%f;%s]"):format(
				view.width + 4, view.height + 4, mainbgcolor)

			formspec = formspec .. view.tablist[view.last_tab_index].get_formspec(
				view,
				view.tablist[view.last_tab_index].name,
				view.tablist[view.last_tab_index].tabdata,
				view.tablist[view.last_tab_index].tabsize)

			formspec = formspec .. "container_end[]"
		end

		return formspec
	end,

	get_header = function(self, view)
		local bgcolor  = view.bgcolor or self.bgcolor
		local selcolor = view.selcolor or self.selcolor
		local mainbgcolor = view.mainbgcolor or self.mainbgcolor

		local last_tab = view.tablist[view.last_tab_index]
		local tsize = last_tab and last_tab.tabsize or {width = view.width, height = view.height}

		local fs = {
			("box[%f,%f;%f,%f;%s]"):format(0, 0, 5.5, tsize.height + 4, mainbgcolor),
			("box[%f,%f;%f,%f;%s]"):format(0, 0, 5.5, tsize.height + 4, bgcolor)
		}

		for i, tab in ipairs(view.tablist) do
			if tab.button_handler then
				local name = "tab_" .. tab.name
				local icon = tab.icon and ESC(defaulttexturedir .. tab.icon)
				local y = (i - 1) * 0.8

				if i == view.last_tab_index then
					fs[#fs + 1] = ("box[%f,%f;%f,%f;%s]"):format(0, y, 5.5, 0.8, selcolor)
				end

				local sel = ESC(defaulttexturedir .. "mainmenu_selected.png")

				if icon then
					fs[#fs + 1] = "image[0.2," .. (y + 0.15) .. ";0.5,0.5;" .. icon .. "]"
				end

				fs[#fs + 1] = "label[0.9,"
				fs[#fs + 1] = y + 0.4
				fs[#fs + 1] = ";"
				fs[#fs + 1] = tab.caption
				fs[#fs + 1] = "]"

				fs[#fs + 1] = "style["
				fs[#fs + 1] = name
				fs[#fs + 1] = ";border=false;bgimg_hovered=" .. sel .. "]"

				if tab.selectable ~= false then
					fs[#fs + 1] = "button[0,"
					fs[#fs + 1] = y
					fs[#fs + 1] = ";5.5,0.8;"
					fs[#fs + 1] = name
					fs[#fs + 1] = ";]"
				end
			end
		end

		fs[#fs + 1] = "image[0.2,7.9;5,1.6;" ..
			ESC(defaulttexturedir .. "header_kidscode_ign.png") .. "]" ..
			"hypertext[0.2,9.3;5,0.5;version;<right><style size=10 color=#888>Version " .. core.get_kidscode_version_string() .. "</style></right>]"

		return table.concat(fs, "")
	end,

	handle_buttons = function(self, view, fields)
		for i = 1, #view.tablist do
			local tab = view.tablist[i]
			if fields["tab_" .. tab.name] then
				view:switch_to_tab(i)
				return true
			end
		end

		return false
	end,
}


tabview_layouts.mainmenu = {
	backtxt = (defaulttexturedir or "") .. "back.png",

	get = function(self, view)
		local tab = view.tablist[view.last_tab_index]
		if tab then
			return self:get_with_back(view, tab)
		else
			return self:get_main_buttons(view)
		end
	end,

	get_with_back = function(self, view, tab)
		local backtxt = view.backtxt or self.backtxt

		local formspec = ""
		if view.parent == nil then
			local tsize = tab.tabsize or
					{width=view.width, height=view.height}
			formspec = formspec ..
					string.format("size[%f,%f,%s]real_coordinates[true]",
						tsize.width, tsize.height, dump(view.fixed_size))
		end
		formspec = formspec ..
				"style[main_back;noclip=true;border=false]" ..
				"image_button[-1.25,0;1,1;" .. backtxt .. ";main_back;]" ..
				tab.get_formspec(
						view,
						view.tablist[view.last_tab_index].name,
						view.tablist[view.last_tab_index].tabdata,
						view.tablist[view.last_tab_index].tabsize
				)

		return formspec
	end,

	get_main_buttons = function(self, view)
		local bgcolor  = view.bgcolor or "#ACACAC33"

		local tsize = {width = 3, height = #view.tablist - 0.2}

		local fs = {
			("size[%f,%f]"):format(tsize.width, tsize.height),
			"real_coordinates[true]",
			"bgcolor[#00000000]",
			("box[%f,%f;%f,%f;%s]"):format(0, 0, 5.5, tsize.height, bgcolor)
		}

		for i = 1, #view.tablist do
			local tab = view.tablist[i]
			local name = "tab_" .. tab.name
			local y = i - 1

			fs[#fs + 1] = "button[0,"
			fs[#fs + 1] = tonumber(y)
			fs[#fs + 1] = ";5.5,0.8;"
			fs[#fs + 1] = name
			fs[#fs + 1] = ";"
			fs[#fs + 1] = ESC(tab.caption)
			fs[#fs + 1] ="]"
		end

		return table.concat(fs, "")
	end,

	handle_buttons = function(self, view, fields)
		if fields.main_back then
			view:switch_to_tab(nil)
			return true
		end
		for i = 1, #view.tablist do
			local tab = view.tablist[i]
			if fields["tab_" .. tab.name] then
				view:switch_to_tab(i)
				return true
			end
		end

		return false
	end,
}
