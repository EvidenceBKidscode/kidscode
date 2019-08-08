/*
Part of Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>
Copyright (C) 2013 RealBadAngel, Maciej Kasatkin <mk@realbadangel.pl>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "mainmenumanager.h"
#include "guiOptions.h"
#include "guiVolumeChange.h"
#include "debug.h"
#include "serialization.h"
#include <string>
#include <IGUICheckBox.h>
#include <IGUIButton.h>
#include <IGUIScrollBar.h>
#include <IGUIStaticText.h>
#include <IGUIFont.h>
#include "settings.h"
#include "gettext.h"

const int ID_soundButton = 364;
const int ID_keysButton = 365;
const int ID_exitButton = 366;

const int ID_mouseSB = 367;
const int ID_viewrangeSB = 368;
const int ID_guiscalingSB = 369;

GUIOptions::GUIOptions(gui::IGUIEnvironment* env,
		gui::IGUIElement* parent, s32 id,
		IMenuManager *menumgr
):
	GUIModalMenu(env, parent, id, menumgr)
{
}

GUIOptions::~GUIOptions()
{
	removeChildren();
}

void GUIOptions::removeChildren()
{
	
}

void GUIOptions::regenerateGui(v2u32 screensize)
{
	/*
		Remove stuff
	*/
	removeChildren();

	/*
		Calculate new sizes and positions
	*/
	DesiredRect = core::rect<s32>(
		screensize.X / 2 - 380 / 2,
		screensize.Y / 8 - 200 / 2,
		screensize.X / 2 + 380 / 2,
		screensize.Y / 8 + 200 / 2
	);
	recalculateAbsolutePosition(false);

	v2s32 size = DesiredRect.getSize();
	float mouse_sensitivity = g_settings->getFloat("mouse_sensitivity");
	float viewing_range = g_settings->getFloat("viewing_range");
	float gui_scaling = g_settings->getFloat("gui_scaling");

	{
		core::rect<s32> rect(0, 0, 160, 40);
		rect += v2s32(size.X / 2 - 180, size.Y - 190);
		const wchar_t *text = wgettext("Sound Volume");
		Environment->addButton(rect, this, ID_soundButton, text);
		delete[] text;
	}

	{
		core::rect<s32> rect(0, 0, 160, 40);
		rect += v2s32(size.X / 2 - 180, size.Y - 140);
		const wchar_t *text = wgettext("Change Keys");
		Environment->addButton(rect, this, ID_keysButton, text);
		delete[] text;
	}

	{
		core::rect<s32> rect(0, 0, 100, 40);
		rect += v2s32(size.X / 2 - 180, size.Y - 50);
		const wchar_t *text = wgettext("Exit");
		Environment->addButton(rect, this, ID_exitButton, text);
		delete[] text;
	}

	{
		core::rect<s32> rect(0, 0, 160, 20);
		rect += v2s32(size.X / 2 + 25, size.Y / 2 - 80);

		const wchar_t *text = wgettext("Mouse sensitivity:");
		core::stringw mouse_sens = text;
		delete [] text;

		Environment->addStaticText(mouse_sens.c_str(), rect, false,
				true, this, 0);
	}

	{
		core::rect<s32> rect(0, 0, 180, 20);
		rect = rect + v2s32(size.X / 2 - 5, size.Y / 2 - 55);
		gui::IGUIScrollBar *e = Environment->addScrollBar(true,
			rect, this, ID_mouseSB);
		e->setMin(0);
		e->setMax(100);
		e->setSmallStep(1);
		e->setLargeStep(10);
		e->setPos(mouse_sensitivity * 100);
	}

	{
		core::rect<s32> rect(0, 0, 160, 20);
		rect += v2s32(size.X / 2 + 25, size.Y / 2 - 25);

		const wchar_t *text = wgettext("Viewing range:");
		core::stringw mouse_sens = text;
		delete [] text;

		Environment->addStaticText(mouse_sens.c_str(), rect, false,
				true, this, 0);
	}

	{
		core::rect<s32> rect(0, 0, 180, 20);
		rect = rect + v2s32(size.X / 2 - 5, size.Y / 2);
		gui::IGUIScrollBar *e = Environment->addScrollBar(true,
			rect, this, ID_viewrangeSB);
		e->setMin(20);
		e->setMax(2000);
		e->setSmallStep(10);
		e->setLargeStep(100);
		e->setPos(viewing_range);
	}

	{
		core::rect<s32> rect(0, 0, 160, 20);
		rect += v2s32(size.X / 2 + 25, size.Y / 2 + 30);

		const wchar_t *text = wgettext("GUI scaling:");
		core::stringw mouse_sens = text;
		delete [] text;

		Environment->addStaticText(mouse_sens.c_str(), rect, false,
				true, this, 0);
	}

	{
		core::rect<s32> rect(0, 0, 180, 20);
		rect = rect + v2s32(size.X / 2 - 5, size.Y / 2 + 55);
		gui::IGUIScrollBar *e = Environment->addScrollBar(true,
			rect, this, ID_guiscalingSB);
		e->setMin(0);
		e->setMax(200);
		e->setSmallStep(10);
		e->setLargeStep(10);
		e->setPos(gui_scaling * 100);
	}
	
}

void GUIOptions::drawMenu()
{
	gui::IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;
	video::IVideoDriver* driver = Environment->getVideoDriver();
	video::SColor bgcolor(140, 0, 0, 0);
	driver->draw2DRectangle(bgcolor, AbsoluteRect, &AbsoluteClippingRect);
	gui::IGUIElement::draw();
}

bool GUIOptions::OnEvent(const SEvent& event)
{
	if (event.EventType == EET_KEY_INPUT_EVENT) {
		if (event.KeyInput.Key == KEY_ESCAPE && event.KeyInput.PressedDown) {
			quitMenu();
			return true;
		}

		if (event.KeyInput.Key == KEY_RETURN && event.KeyInput.PressedDown) {
			quitMenu();
			return true;
		}
	} else if (event.EventType == EET_GUI_EVENT) {
		if (event.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
			if (event.GUIEvent.Caller->getID() == ID_soundButton) {
				g_gamecallback->changeVolume();
				return true;
			}
			Environment->setFocus(this);
		}

		if (event.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
			if (event.GUIEvent.Caller->getID() == ID_keysButton) {
				g_gamecallback->keyConfig();
				return true;
			}
			Environment->setFocus(this);
		}

		if (event.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
			if (event.GUIEvent.Caller->getID() == ID_exitButton) {
				quitMenu();
				return true;
			}
			Environment->setFocus(this);
		}

		if (event.GUIEvent.EventType == gui::EGET_ELEMENT_FOCUS_LOST
				&& isVisible()) {
			if (!canTakeFocus(event.GUIEvent.Element)) {
				dstream << "GUIMainMenu: Not allowing focus change."
				<< std::endl;
				// Returning true disables focus change
				return true;
			}
		}

		if (event.GUIEvent.EventType == gui::EGET_SCROLL_BAR_CHANGED) {
			if (event.GUIEvent.Caller->getID() == ID_mouseSB) {
				s32 pos = ((gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
				g_settings->setFloat("mouse_sensitivity", (float)pos / 100);
				return true;
			} else if (event.GUIEvent.Caller->getID() == ID_viewrangeSB) {
				s32 pos = ((gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
				g_settings->setU16("viewing_range", (float)pos);
				return true;
			} else if (event.GUIEvent.Caller->getID() == ID_guiscalingSB) {
				s32 pos = ((gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
				g_settings->setFloat("gui_scaling", (float)pos / 100);
				return true;
			}
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}
