/*
Minetest
Copyright (C) 2019 EvicenceBKidscode / Pierre-Yves Rollo <dev@pyrollo.com>

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

#include "IGUIEnvironment.h"
#include "IGUIElement.h"
#include "guiScrollBar.h"
#include "IGUIFont.h"
#include <vector>
#include <list>
#include <unordered_map>
#include <cstdlib>
using namespace irr::gui;
#include "fontengine.h"
#include <SColor.h>
#include "client/tile.h"
#include "IVideoDriver.h"
#include "client.h"
#include "client/renderingengine.h"
#include "hud.h"
#include "guiText.h"

//! constructor
GUIText::GUIText(
	const wchar_t* text,
	IGUIEnvironment* environment,
	IGUIElement* parent, s32 id,
	const core::rect<s32>& rectangle,
	Client *client,
	ISimpleTextureSource *tsrc) :
	IGUIElement(EGUIET_ELEMENT, environment, parent, id, rectangle),
	m_tsrc(tsrc),
	m_client(client),
	m_vscrollbar(NULL),
	m_text_scrollpos(0, 0)
{
	#ifdef _DEBUG
		setDebugName("GUIText");
	#endif

	m_raw_text = text;

	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();

	m_scrollbar_width = skin ? skin->getSize(gui::EGDS_SCROLLBAR_SIZE) : 16;

	m_vscrollbar = new GUIScrollBar(Environment, false, this, -1,
		irr::core::rect<s32>(
			RelativeRect.getWidth() - m_scrollbar_width,
			0,
			RelativeRect.getWidth(),
			RelativeRect.getHeight()
		)
	);

	m_vscrollbar->setVisible(false);
	m_vscrollbar->setSmallStep(1);
	m_vscrollbar->setLargeStep(100);
	m_vscrollbar->setPos(0);
	m_vscrollbar->setMax(1);
}

//! destructor
GUIText::~GUIText()
{
	m_vscrollbar->remove();
}


void GUIText::checkHover(s32 X, s32 Y) {
	GUIText::fragment * fragment = getFragmentAt(X, Y);
	m_hover_tag_id = 0;
	if (fragment) {
		for (auto id : fragment->tag_ids) {
			if (m_tags[id].name == "link") {
				m_hover_tag_id = id;
				break;
			}
		}
	}
	if (m_hover_tag_id)
		RenderingEngine::get_raw_device()->getCursorControl()->setActiveIcon(gui::ECI_HAND);
		else
		RenderingEngine::get_raw_device()->getCursorControl()->setActiveIcon(gui::ECI_NORMAL);
}

bool GUIText::OnEvent(const SEvent& event) {
	if (event.EventType == EET_GUI_EVENT) {
		if (event.GUIEvent.EventType == EGET_SCROLL_BAR_CHANGED)
			if (event.GUIEvent.Caller == m_vscrollbar)
				m_text_scrollpos.Y = -m_vscrollbar->getPos();
	}

	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		if (event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
			m_vscrollbar->setPos(m_vscrollbar->getPos() -
					event.MouseInput.Wheel * m_vscrollbar->getLargeStep());
			m_text_scrollpos.Y = -m_vscrollbar->getPos();
			draw(m_parsed_text, m_display_text_rect);
			checkHover(event.MouseInput.X,event.MouseInput.Y);
		} else if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
			GUIText::fragment * fragment = getFragmentAt(event.MouseInput.X,event.MouseInput.Y);
			if (fragment) {
				for (auto id : fragment->tag_ids) {
					if (m_tags[id].name == "link") {
					 	if (m_tags[id].link != "") {
							Text = std::wstring(m_tags[id].link.begin(), m_tags[id].link.end()).c_str();
							Text = core::stringw(L"link:") + Text;
							if (Parent)
							{
								SEvent newEvent;
								newEvent.EventType = EET_GUI_EVENT;
								newEvent.GUIEvent.Caller = this;
								newEvent.GUIEvent.Element = 0;
								newEvent.GUIEvent.EventType = EGET_BUTTON_CLICKED;
								Parent->OnEvent(newEvent);
							}
						}
						break;
					}
				}
			}
			// TODO: Does not receive event if not foccused
		} else if (event.MouseInput.Event == EMIE_MOUSE_MOVED) {
			checkHover(event.MouseInput.X, event.MouseInput.Y);
		}
	}

	return IGUIElement::OnEvent(event);
}

//! draws the element and its children
void GUIText::draw()
{
	if (!IsVisible)
		return;

	// Frame
	IGUISkin* skin = Environment->getSkin();
	if (skin)
		skin->draw3DSunkenPane(this, video::SColor(0), false, false,
				AbsoluteRect, &AbsoluteClippingRect);

	// Text
	m_display_text_rect = AbsoluteRect;
	m_display_text_rect.UpperLeftCorner.X ++;
	m_display_text_rect.UpperLeftCorner.Y ++;
	m_display_text_rect.LowerRightCorner.X --;
	m_display_text_rect.LowerRightCorner.Y --;
	parse();
	size(m_parsed_text);
	place(m_parsed_text, m_display_text_rect.getWidth());

	if (m_parsed_text.height > m_display_text_rect.getHeight()) {

		m_vscrollbar->setMax(m_parsed_text.height - m_display_text_rect.getHeight());
		m_vscrollbar->setVisible(true);

		core::rect<s32> smaller_rect = m_display_text_rect;
		smaller_rect.LowerRightCorner.X -= m_scrollbar_width;
		place(m_parsed_text, smaller_rect.getWidth());

	} else {
		// TODO: vertical adjust
		m_vscrollbar->setVisible(false);
	}
	draw(m_parsed_text, m_display_text_rect);

	// draw children
	IGUIElement::draw();
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------

//TODO:Have that in global style
irr::video::SColor hovercolor(255, 255, 0, 0);
irr::video::SColor linkcolor(255, 0, 0, 255);

void GUIText::draw(
	GUIText::text & text,
	core::rect<s32> & text_rect)
{
	core::position2d<s32> offset = text_rect.UpperLeftCorner + m_text_scrollpos;

	for (auto & paragraph : text.paragraphs) {
		for (auto & word : paragraph.words)
		{
			if (word.type == t_word) {
				for (auto & fragment : word.fragments)
				{
					core::rect<s32> rect(fragment.position + offset, fragment.dimension);

					irr::video::SColor color = fragment.color;
					for (auto id:fragment.tag_ids) {
						if (id && id == m_hover_tag_id) {
							color = hovercolor;
							break;
						}
						if (m_tags[id].name == "link") {
							color = linkcolor;
							break;
						}
					}

					if (rect.isRectCollided(text_rect) && fragment.font)
						fragment.font->draw(fragment.text, rect,
							color, false, true, &text_rect);
				}

			} else if (word.type == t_image) {
				video::ITexture *texture = m_tsrc->getTexture(word.name);
				if (texture != 0)
				{
					core::rect<s32> rect(word.position + offset, word.dimension);
					if (rect.isRectCollided(text_rect))
					{
						Environment->getVideoDriver()->draw2DImage(texture,
							rect, // Dest rect
							irr::core::rect<s32>(core::position2d<s32>(0, 0), texture->getOriginalSize()), // Source rect
							&text_rect, // Clip rect
							0, true); // Colors, use alpha
					}
				} else printf("Texture %s not found!\n", word.name.c_str());

			} else if (word.type == t_item) {
				core::rect<s32> rect(word.position + offset, word.dimension);
				if (rect.isRectCollided(text_rect))
				{
					IItemDefManager *idef = m_client->idef();
					ItemStack item;
					item.deSerialize(word.name, idef);

					drawItemStack(
							Environment->getVideoDriver(), g_fontengine->getFont(), item,
							rect, &text_rect, m_client, word.rotation);
					// TODO: Add IT_ROT_SELECTED possibility
				}
			}
		}
	}
}

GUIText::word* GUIText::getWordAt(s32 X, s32 Y)
{
	core::position2d<s32> pos(X, Y);
	pos -= m_text_scrollpos;

	for (auto & paragraph : m_parsed_text.paragraphs) {
		for (auto & word : paragraph.words) {
			core::rect<s32> rect(word.position, word.dimension);
			if (rect.isPointInside(pos))
				return &word;
		}
	}
	return 0;
}

GUIText::fragment* GUIText::getFragmentAt(s32 X, s32 Y)
{
	core::position2d<s32> pos(X, Y);
	pos -= m_text_scrollpos;

	for (auto & paragraph : m_parsed_text.paragraphs) {
		for (auto & word : paragraph.words) {
			for (auto & fragment : word.fragments) {
				core::rect<s32> rect(fragment.position, fragment.dimension);
				if (rect.isPointInside(pos))
					return &fragment;
				}
		}
	}
	return 0;
}
// -----------------------------------------------------------------------------
// Placing
// -----------------------------------------------------------------------------

void GUIText::size(GUIText::text &text)
{
	for (auto & paragraph : text.paragraphs) {
		for (auto & word : paragraph.words) {
			if (word.type == t_separator || word.type == t_word) {
				word.dimension.Height = 0;
				word.dimension.Width = 0;
				for (auto & fragment : word.fragments) {
					if (fragment.font)
						fragment.dimension =
							fragment.font->getDimension(fragment.text.c_str());
					else
						fragment.dimension = {0, 0};

					if (word.dimension.Height < fragment.dimension.Height)
						word.dimension.Height = fragment.dimension.Height;
					word.dimension.Width += fragment.dimension.Width;
				}
			} else if (word.type == t_image || word.type == t_item) {
				// Dont resize already sized items (sized by another mechanism)
				if (word.dimension.Height == 0 || word.dimension.Width == 0) {
					core::dimension2d<u32> dim(80, 80); // Default image and item size

					if (word.type == t_image) {
						video::ITexture *texture = m_tsrc->getTexture(word.name);
						if (texture != 0)
							dim = texture->getOriginalSize();
					}

					if (word.dimension.Height == 0)
						if (word.dimension.Width == 0)
							word.dimension = dim;
						else
							word.dimension.Height = dim.Height * word.dimension.Width / dim.Width;
					else
						word.dimension.Width = dim.Width * word.dimension.Height / dim.Height;
				}
			}
		}
	}
}

// Place text in width, with no text margin
void GUIText::place(
	GUIText::text & text,
	s32 width)
{
	m_floating.clear();
	s32 y = 0;
	s32 ymargin = text.margin;

	for (auto & paragraph : text.paragraphs) {
		paragraph.height = 0;

		// Find and place floating stuff in paragraph
		for (auto word = paragraph.words.begin(); word != paragraph.words.end();
				++word) {
			if (word->floating != floating_none) {
				if (y)
					word->position.Y = y + std::max(ymargin, word->margin);
				else
					word->position.Y = ymargin;

				if (word->floating == floating_left)
					word->position.X = text.margin;
				if (word->floating == floating_right)
					word->position.X = width - word->dimension.Width - text.margin;

				rect_with_margin floating;
				floating.rect = core::rect<s32>(word->position, word->dimension);
				floating.margin = word->margin;

				m_floating.push_back(floating);
			}
		}

		if (y)
			y = y + std::max(ymargin, paragraph.margin);

		ymargin = paragraph.margin;

		// Place non floating stuff
		std::vector<GUIText::word>::iterator current_word = paragraph.words.begin();
		while (current_word != paragraph.words.end())
	 	{
			// Determine line width and y pos
			s32 left, right;
			s32 nexty = y;
			do {
				y = nexty;
				nexty = 0;

				// Inner left & right
				left = text.margin;
				right = width - text.margin;

				for (auto floating : m_floating) {
					// Is this floating rect interecting paragraph y line ?
					if (floating.rect.UpperLeftCorner.Y - floating.margin <= y &&
							floating.rect.LowerRightCorner.Y + floating.margin >= y) {

						// Next Y to try if no room left
						if (!nexty || floating.rect.LowerRightCorner.Y + std::max(floating.margin, paragraph.margin) < nexty)
							nexty = floating.rect.LowerRightCorner.Y + std::max(floating.margin, paragraph.margin) + 1;

						if (floating.rect.UpperLeftCorner.X - floating.margin <= left &&
								floating.rect.LowerRightCorner.X + floating.margin < right)
						{ // float on left
							if (floating.rect.LowerRightCorner.X +
									std::max(floating.margin, paragraph.margin) > left)
								left = floating.rect.LowerRightCorner.X +
										std::max(floating.margin, paragraph.margin);

						} else if (floating.rect.LowerRightCorner.X + floating.margin >= right &&
								floating.rect.UpperLeftCorner.X - floating.margin > left)
						{ // float on right
							if (floating.rect.UpperLeftCorner.X -
									std::max(floating.margin, paragraph.margin) < right)
								right = floating.rect.UpperLeftCorner.X -
										std::max(floating.margin, paragraph.margin);

						} else if (floating.rect.UpperLeftCorner.X - floating.margin <= left &&
							 floating.rect.LowerRightCorner.X + floating.margin >= right)
						{ // float taking all space
							left = right;

						} else { // float in the middle -- should not occure yet, see that later
						}
					}
				}
			} while (nexty && right <= left);

			u32 linewidth = right - left;
			float x = left;

			while(current_word != paragraph.words.end() &&
					current_word->type == t_separator) {
				current_word++;
			}

			s32 charswidth = 0;
			s32 charsheight = 0;
			u32 wordcount = 0;

			std::vector<GUIText::word>::iterator linestart = current_word;
			std::vector<GUIText::word>::iterator lineend = paragraph.words.end();

			// First pass, find words fitting into line (or at least one world)
			while(current_word != paragraph.words.end() &&
					(charswidth == 0 || charswidth + current_word->dimension.Width <= linewidth))
			{
				if (current_word->floating == floating_none)
				{
					if (current_word->type != t_separator)
					{
						lineend = current_word;
						wordcount++;
					}
					charswidth += current_word->dimension.Width;
				}
				current_word++;
			}

			// Empty line, nothing to place or draw
			if (lineend == paragraph.words.end()) continue;

			lineend++; // Point to the first position outside line (may be end())

			// Second pass, compute printable line width and adjustments
			charswidth = 0;
			for(auto word = linestart; word != lineend; ++word) {
				if (word->floating == floating_none) {
					charswidth += word->dimension.Width;
					if (charsheight < word->dimension.Height)
						charsheight = word->dimension.Height;
				}
			}

			float extraspace = 0;

			switch(paragraph.halign)
			{
				case h_center:
					x += (linewidth - charswidth) / 2;
					break;
				case h_justify:
					if (wordcount > 1 && // Justification only if at least two words.
						!(lineend == paragraph.words.end())) // Don't justify last line.
						extraspace = ((float)(linewidth - charswidth)) / (wordcount - 1);
					break;
				case h_right:
					x += linewidth - charswidth;
			}

			// Third pass, actually place everything
			for(auto word = linestart; word != lineend; ++word)
			{
				if (word->floating == floating_none)
				{
					word->position.X = x;
					word->position.Y = y;

					if (word->type == t_word || word->type == t_separator)
					{

						word->dimension.Height = charsheight;

						for (auto fragment = word->fragments.begin();
							fragment != word->fragments.end(); ++fragment)
						{
							fragment->position.X = x;

							switch(fragment->valign)
							{
								case v_top:
									fragment->position.Y = y;
									break;
								case v_middle:
									fragment->position.Y = y + (charsheight - fragment->dimension.Height) / 2 ;
									break;
								default:
									fragment->position.Y = y + charsheight - fragment->dimension.Height ;
							}
							x += fragment->dimension.Width;
						}
						if (word->type == t_separator)
							x += extraspace;
					} else if (word->type == t_image || word->type == t_item) {
						x += word->dimension.Width;
					}
				}
			}
			paragraph.height += charsheight;
			y += charsheight;
		}
	}

	// Check if float goes under paragraph
	for (auto floating : m_floating) {
		if (floating.rect.LowerRightCorner.Y >= y)
			y = floating.rect.LowerRightCorner.Y;
	}

	text.height = y + text.margin;
}

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------

// Code snippet from Galik at https://stackoverflow.com/questions/38812780/split-string-into-key-value-pairs-using-c

GUIText::KeyValues GUIText::get_attributes(std::string const& s)
{
	GUIText::KeyValues m;

	std::string::size_type key_pos = 0;
	std::string::size_type key_end;
	std::string::size_type val_pos;
	std::string::size_type val_end;

	while((key_end = s.find('=', key_pos)) != std::string::npos)
	{
		if((val_pos = s.find_first_not_of("= ", key_end)) == std::string::npos)
			break;

		val_end = s.find(' ', val_pos);
		m.emplace(s.substr(key_pos, key_end - key_pos), s.substr(val_pos, val_end - val_pos));

		key_pos = val_end;
		if(key_pos != std::string::npos)
			++key_pos;
	}

	return m;
}

char getwcharhexdigit(char c)
{
	if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) return (char)c;
	if (c >= 'a' && c <= 'f') return (c - 32);
	return 0;
}

bool format_color(std::string source, std::string &target) {
	if (source[0] != '#') return false;
	std::string color = "";
	char c;
	if (source.size() == 4) {
			if (!(c = getwcharhexdigit(source[1]))) return false;
			color += c; color += c;
			if (!(c = getwcharhexdigit(source[2]))) return false;
			color += c; color += c;
			if (!(c = getwcharhexdigit(source[3]))) return false;
			color += c; color += c;
	} else if (source.size() == 7) {
			if (!(c = getwcharhexdigit(source[1]))) return false;
			color += c;
			if (!(c = getwcharhexdigit(source[2]))) return false;
			color += c;
			if (!(c = getwcharhexdigit(source[3]))) return false;
			color += c;
			if (!(c = getwcharhexdigit(source[4]))) return false;
			color += c;
			if (!(c = getwcharhexdigit(source[5]))) return false;
			color += c;
			if (!(c = getwcharhexdigit(source[6]))) return false;
			color += c;
	} else return false;
	target = color;
	return true;
}

void GUIText::fragment::set_style(KeyValues &style) {
	if (style["valign"] == "middle")
		this->valign = v_middle;
	else if (style["valign"] == "top")
		this->valign = v_top;
	else
		this->valign = v_bottom;

	int r, g, b;
	sscanf(style["color"].c_str(),"%2x%2x%2x", &r, &g, &b);
	this->color = irr::video::SColor(255, r, g, b);

	unsigned int font_size = std::atoi(style["fontsize"].c_str());
	FontMode font_mode = FM_Standard;
	if (style["fontstyle"] == "mono")
		font_mode = FM_Mono;

	// TODO : Here server can crash depending on user entry :/
	this->font = g_fontengine->getFont(font_size, font_mode);
	if (!this->font)
		printf("No font found ! Size=%d, mode=%d\n", font_size, font_mode);
}

void GUIText::paragraph::set_style(KeyValues &style) {
	if (style["halign"] == "center")
		this->halign = h_center;
	else if (style["halign"] == "right")
		this->halign = h_right;
	else if (style["halign"] == "justify")
		this->halign = h_justify;
	else
		this->halign = h_left;
}

void GUIText::update_style()
{
	m_current_style.clear();

 	for (auto id = m_active_tag_ids.crbegin(); id != m_active_tag_ids.crend(); ++id) {
		for (auto const& prop : m_tags[*id].style) {
			m_current_style[prop.first] = prop.second;
		}
	}
}

void GUIText::end_fragment()
{
	if (!m_current_fragment) return;
	m_current_fragment = NULL;
}

void GUIText::end_word()
{
	if (!m_current_word) return;

	end_fragment();
	m_current_word = NULL;
}

void GUIText::end_paragraph()
{
	if (!m_current_paragraph) return;

	end_word();
	m_current_paragraph = NULL;
}

void GUIText::enter_paragraph()
{
	if (!m_current_paragraph) {
		m_parsed_text.paragraphs.emplace_back();
		m_current_paragraph = & m_parsed_text.paragraphs.back();
		m_current_paragraph->set_style(m_current_style);
	}
}

void GUIText::enter_word(wordtype type)
{
	enter_paragraph();

	if (m_current_word and m_current_word->type != type)
		end_word();

	if (!m_current_word) {
		m_current_paragraph->words.emplace_back();
		m_current_word = & m_current_paragraph->words.back();
		m_current_word->type = type;
	}
}

void GUIText::enter_fragment(wordtype type)
{
	enter_word(type);

	if (!m_current_fragment) {
		m_current_word->fragments.emplace_back();
		m_current_fragment = & m_current_word->fragments.back();
		m_current_fragment->set_style(m_current_style);
		m_current_fragment->tag_ids = m_active_tag_ids;
	}
}

void GUIText::push_char(wchar_t c)
{
	// New word if needed
	if (c == L' ' || c == L'\t')
		enter_fragment(t_separator);
	else
		enter_fragment(t_word);

	m_current_fragment->text += c;
}

u32 GUIText::parse_tag(u32 cursor)
{
	u32 textsize = m_raw_text.size();
	bool tag_end = false;
	bool tag_start = false;
	std::string tag_param = "";
	markup_tag tag;
	wchar_t c;

	if (cursor >= textsize) return 0;
	c = m_raw_text[cursor];

	if (c == L'/') {
		tag_end = true;
		if (++cursor >= textsize) return 0;
		c = m_raw_text[cursor];
	}

	while (c != ' ' && c != '>')
	{
		tag.name += (char)c;
		if (++cursor >= textsize) return 0;
		c = m_raw_text[cursor];
	}

	while (c == ' ') {
		if (++cursor >= textsize) return 0;
		c = m_raw_text[cursor];
	}

	while (c != '>') {
		tag_param += (char)c;
		if (++cursor >= textsize) return 0;
		c = m_raw_text[cursor];
	}

	++cursor; // last '>'

	tag.attrs = get_attributes(tag_param);

	KeyValues style;

	if (tag.name == "link") {
		if (!tag_end) {
			if (!tag.attrs.count("name"))
				return 0;
			tag.link = tag.attrs["name"];
			tag_start = true;
		}
	} else if (tag.name == "img" || tag.name == "item") {
		if (tag_end) return 0;

		if (tag.name == "img")
			enter_word(t_image);
		else
			enter_word(t_item);

		// Required attributes
		if (!tag.attrs.count("name"))
			return 0;
		m_current_word->name = tag.attrs["name"];

		if (tag.attrs.count("float")) {
			if (tag.attrs["float"] == "left") m_current_word->floating = floating_left;
			if (tag.attrs["float"] == "right") m_current_word->floating = floating_right;
		}

		if (tag.attrs.count("rotate")) {
			 if (tag.attrs["rotate"] == "yes") m_current_word->rotation = IT_ROT_SELECTED;
		}

		if (tag.attrs.count("width")) {
			int width = strtol(tag.attrs["width"].c_str(), NULL, 10);
			if (width)
				m_current_word->dimension.Width = width;
		}

		if (tag.attrs.count("height")) {
			int height = strtol(tag.attrs["height"].c_str(), NULL, 10);
			if (height)
				m_current_word->dimension.Height = height;
		}

		end_word();

	} else if (tag.name == "center") {
		if (!tag_end) {
			tag.style["halign"] = "center";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag.name == "justify") {
		if (!tag_end) {
			tag.style["halign"] = "justify";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag.name == "left") {
		if (!tag_end) {
			tag.style["halign"] = "left";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag.name == "right") {
		if (!tag_end) {
			tag.style["halign"] = "right";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag.name == "normal") {
		if (!tag_end) {
			tag.style["fontsize"] = "16";
			tag_start = true;
		}
		end_fragment();
	} else if (tag.name == "big") {
		if (!tag_end) {
			tag.style["fontsize"] = "24";
			tag_start = true;
		}
		end_fragment();
	} else if (tag.name == "bigger") {
		if (!tag_end) {
			tag.style["fontsize"] = "36";
			tag_start = true;
		}
		end_fragment();
	} else if (tag.name == "style") {
		if (!tag_end) {
			if (tag.attrs.count("color")) {
				std::string color;
				if (!format_color(tag.attrs["color"], color)) return 0;
				tag.style["color"] = color;
			}
			if (tag.attrs.count("font")) {
				if (tag.attrs["font"] == "mono" || tag.attrs["font"] == "normal")
					tag.style["fontstyle"] = tag.attrs["font"];
				else
					return 0;
			}
			if (tag.attrs.count("size")) {
				int size = strtol(tag.attrs["size"].c_str(), NULL, 10);
				if (size <= 0) return 0;
				tag.style["fontsize"] = std::to_string(size);
			}
			tag_start = true;
		}
		end_fragment();
	} else return 0; // Unknown tag

	if (tag_start) {
		// tag index is used as tag id
		s32 id = m_tags.size();
		m_tags.push_back(tag);
		m_active_tag_ids.push_front(id);
		update_style();
	}
	// Tag end, unstack last stacked tag with same name
	if (tag_end) {
		bool found = false;
		for (auto id = m_active_tag_ids.begin(); id != m_active_tag_ids.end(); ++id)
			if (m_tags[*id].name == tag.name) {
				m_active_tag_ids.erase(id);
				found = true;
				break;
			}
		if (!found) return 0;
		update_style();
	}
	return cursor;
}

void GUIText::parse()
{
	m_tags.clear();
	m_active_tag_ids.clear();
	markup_tag root_tag;
	root_tag.name = "root";
	root_tag.link = "";
	root_tag.style["fontsize"] = "16";
	root_tag.style["fontstyle"] = "normal";
	root_tag.style["halign"] = "left";
	root_tag.style["color"] = "FFFFFF";
	m_tags.push_back(root_tag);
	m_active_tag_ids.push_front(0);
	update_style();

	m_parsed_text.paragraphs.clear();
	m_current_fragment = NULL;
	m_current_word = NULL;
	m_current_paragraph = NULL;

	u32 cursor = 0;
	bool escape = false;
	wchar_t c;
	u32 textsize = m_raw_text.size();
	while (cursor < textsize)
	{
		c = m_raw_text[cursor];
		cursor++;

		if (c == L'\r') { // Mac or Windows breaks
			if (cursor < textsize && m_raw_text[cursor] == L'\n')
				cursor++;
			end_paragraph();
			escape = false;
			continue;
		}

		if (c == L'\n') { // Unix breaks
			end_paragraph();
			escape = false;
			continue;
		}

		if (escape) {
			escape = false;
			push_char(c);
			continue;
		}

		if (c == L'\\') {
			escape = true;
			continue;
		}
		// Tag check
		if (c == L'<') {
			u32 newcursor = parse_tag(cursor);
			if (newcursor > 0) {
				cursor = newcursor;
				continue;
			}
		}

		// Default behavior
		push_char(c);
	}
	end_paragraph();
}
