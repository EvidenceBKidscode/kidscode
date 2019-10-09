/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <IGUIStaticText.h>
#include "client.h"
#include "client/gamemenu.h"
#include "client/inputhandler.h"
#include "client/renderingengine.h"
#include "client/joystick_controller.h"
#include "gui/guiFormSpecMenu.h"
#include "gui/guiVolumeChange.h"
#include "gui/guiPasswordChange.h"
#include "gui/guiKeyChangeMenu.h"
#include "gui/guiOptions.h"
#include "gui/mainmenumanager.h"
#include "settings.h"

#define PAUSE_TIMER_LIMIT 10.f

struct GameMenuFormspecHandler : public TextDest
{
	void gotText(const StringMap &fields)
	{
		if (fields.find("btn_options") != fields.end()) {
			g_gamecallback->showOptions();
			return;
		}

		if (fields.find("btn_exit_menu") != fields.end()) {
			g_gamecallback->disconnect();
			return;
		}

		if (fields.find("btn_exit_os") != fields.end()) {
			g_gamecallback->exitToOS();
#ifndef __ANDROID__
			RenderingEngine::get_raw_device()->closeDevice();
#endif
			return;
		}

		if (fields.find("quit") != fields.end())
			return;

		if (fields.find("btn_continue") != fields.end())
			return;

		if (fields.find("sbr_mouse_sensitivity") != fields.end()) {
			float val = 0.0f;
			for (auto &v : fields) {
				if (v.first == "sbr_mouse_sensitivity") {
					val = stof(v.second.substr(v.second.find(":") + 1, -1)) * 0.002f;
					g_settings->setFloat("mouse_sensitivity", val);
					break;
				}
			}
		}

		if (fields.find("sbr_gui_scaling") != fields.end()) {
			float val = 0.0f;
			for (auto &v : fields) {
				if (v.first == "sbr_gui_scaling") {
					val = stof(v.second.substr(v.second.find(":") + 1, -1)) * 0.002f;
					g_settings->setFloat("gui_scaling", val);
					break;
				}
			}
		}

		if (fields.find("sbr_viewing_range") != fields.end()) {
			float val = 0.0f;
			for (auto &v : fields) {
				if (v.first == "sbr_viewing_range") {
					val = (stof(v.second.substr(v.second.find(":") + 1, -1)) * 100.0f) / 250.0f;
					g_settings->setFloat("viewing_range", val);
					break;
				}
			}
		}
	}
};

GameMenu::GameMenu(GUIFormSpecMenu **current_formspec, InputHandler *input, Client *client):
		m_current_formspec(current_formspec),
		m_input(input),
		m_client(client) {
			
	// Prepare Pause Menu stuff
	std::ostringstream os;
	os << FORMSPEC_VERSION_STRING << "size[5.6,1.1,true]";

	f32 xpos = -0.15f;

	os << "button_exit[" << (xpos) << ",0;3,0.5;btn_continue;"
		 << strgettext("Continue") << "]";

	os << "button_exit[" << (xpos) << ",0.8;3,0.5;btn_options;"
		 << strgettext("Options") << "]";

	os << "button_exit[" << (xpos+=2.9f) << ",0;3,0.5;btn_exit_menu;"
		 << strgettext("Exit to Menu") << "]";

	os << "button_exit[" << (xpos) << ",0.8;3,0.5;btn_exit_os;"
		 << strgettext("Exit to OS") << "]";

	m_pausemenu_fs = os.str();
	
	std::ostringstream os2;
	os2 << FORMSPEC_VERSION_STRING << "size[0,0,true]";
	os2 << "bgcolor[#00000000;true]";

	m_pausehidden_fs = os2.str();
		
}

void GameMenu::setFormspec(std::string fs) {
	if (*m_current_formspec == 0) {
		*m_current_formspec = new GUIFormSpecMenu(&m_input->joystick, guiroot, -1,
			&g_menumgr, m_client, m_client->getTextureSource(), 
			new FormspecFormSource(fs), new GameMenuFormspecHandler());
	} else {
		(*m_current_formspec)->setFormSource(new FormspecFormSource(fs));
		(*m_current_formspec)->setTextDest(new GameMenuFormspecHandler());
	}
	(*m_current_formspec)->doPause = true;
}

/* returns false if game should exit, otherwise true
 */
bool GameMenu::handleCallbacks()
{
	if (g_gamecallback->disconnect_requested) {
		g_gamecallback->disconnect_requested = false;
		return false;
	}

	if (g_gamecallback->changepassword_requested) {
		m_event_timer = 0.f;
		(new GUIPasswordChange(guienv, guiroot, -1, &g_menumgr, m_client))->drop();
		g_gamecallback->changepassword_requested = false;
	}

	if (g_gamecallback->changevolume_requested) {
		m_event_timer = 0.f;
		(new GUIVolumeChange(guienv, guiroot, -1, &g_menumgr))->drop();
		g_gamecallback->changevolume_requested = false;
	}

	if (g_gamecallback->keyconfig_requested) {
		m_event_timer = 0.f;
		(new GUIKeyChangeMenu(guienv, guiroot, -1, &g_menumgr))->drop();
		g_gamecallback->keyconfig_requested = false;
	}

	if (g_gamecallback->options_requested) {
		m_event_timer = 0.f;
		(new GUIOptions(guienv, guiroot, -1, &g_menumgr))->drop();
		g_gamecallback->options_requested = false;
	}

	if (g_gamecallback->show_pause_menu) {
		m_event_timer = 0.f;
		setFormspec(m_pausemenu_fs);
		g_gamecallback->show_pause_menu = false;
	}

	return true;
}

void GameMenu::active_event(f32 dtime, bool active)
{
	if (active) {
		m_event_timer = 0.f;
		g_menumgr.setVisible(true);
	} else {
		m_event_timer += dtime;
		if (m_event_timer > PAUSE_TIMER_LIMIT)
			g_menumgr.setVisible(false);
	}
}