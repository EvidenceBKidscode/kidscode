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
#include <cwchar>
using namespace irr::gui;
#include "client/fontengine.h"
#include <SColor.h>
#include "client/tile.h"
#include "IVideoDriver.h"
#include "client/client.h"
#include "client/renderingengine.h"
#include "hud.h"
#include "guiHyperText.h"

std::string strwtostr(const core::stringw &str) {
	std::string text = core::stringc(str.c_str()).c_str();
	return text;
}

irr::core::stringw strtostrw(const std::string &str) {
	size_t size = str.size();
	wchar_t *text = new wchar_t[size + sizeof(wchar_t)]; //s.size() doesn't include NULL terminator
	const char *data = &str[0];

	mbsrtowcs(text, &data, size, NULL);

	text[size] = L'\0';
	return text;
}

bool check_color(const std::string &str) {
	irr::video::SColor color;
	return parseColorString(str, color, false);
}

bool check_integer(const std::string &str) {
	if (str == "")
		return false;

	char *endptr = nullptr;
	strtol(str.c_str(), &endptr, 10);

	return *endptr == '\0';
}

// -----------------------------------------------------------------------------
// ParsedText - A text parser

void ParsedText::Element::setStyle(StyleList &style)
{
	if (style["valign"] == "middle")
		this->valign = VALIGN_MIDDLE;
	else if (style["valign"] == "top")
		this->valign = VALIGN_TOP;
	else
		this->valign = VALIGN_BOTTOM;

	video::SColor color;
	if (parseColorString(style["color"], color, false))
		this->color = color;
	if (parseColorString(style["hovercolor"], color, false))
		this->hovercolor = color;

	unsigned int font_size = std::atoi(style["fontsize"].c_str());
	FontMode font_mode = FM_Standard;

	if (style["fontstyle"] == "mono") 
		font_mode = FM_Mono;
	else if (style["fontstyle"] == "italic")
		font_mode = FM_Italic;
	else if (style["fontstyle"] == "bold")
		font_mode = FM_Bold;

	// TODO: find a way to check font validity
	// Build a new fontengine ?
	this->font = g_fontengine->getFont(font_size, font_mode);
	if (!this->font)
		printf("No font found ! Size=%d, mode=%d\n", font_size, font_mode);
}

void ParsedText::Paragraph::setStyle(StyleList &style)
{
	if (style["halign"] == "center")
		this->halign = HALIGN_CENTER;
	else if (style["halign"] == "right")
		this->halign = HALIGN_RIGHT;
	else if (style["halign"] == "justify")
		this->halign = HALIGN_JUSTIFY;
	else
		this->halign = HALIGN_LEFT;
}

ParsedText::ParsedText(const wchar_t *text)
{
	// Default style

	m_root_tag.name = "root";
	m_root_tag.style["fontsize"] = "16";
	m_root_tag.style["fontstyle"] = "normal";
	m_root_tag.style["halign"] = "left";
	m_root_tag.style["color"] = "#EEEEEE";
	m_root_tag.style["hovercolor"] = m_root_tag.style["color"];

	m_tags.push_back(&m_root_tag);
	m_active_tags.push_front(&m_root_tag);
	m_style = m_root_tag.style;

	// Default simple tags definitions

	StyleList style;

	style["hovercolor"] = "#FF0000";
	style["color"] = "#00FF00";
	m_elementtags["action"] = style;
	style.clear();

	style["color"] = "#FFFFFF";
	m_elementtags["b"] = style;
	style.clear();

	style["color"] = "#CCCCCC";
	m_elementtags["i"] = style;
	style.clear();

	style["fontstyle"] = "mono";
	m_elementtags["mono"] = style;
	style.clear();

	style["fontstyle"] = "italic";
	m_elementtags["italic"] = style;
	style.clear();

	style["fontstyle"] = "bold";
	m_elementtags["bold"] = style;
	style.clear();

	style["fontsize"] = m_root_tag.style["fontsize"];
	m_elementtags["normal"] = style;
	style.clear();

	style["fontsize"] = "24";
	m_elementtags["big"] = style;
	style.clear();

	style["fontsize"] = "36";
	m_elementtags["bigger"] = style;
	style.clear();

	style["halign"] = "center";
	m_paragraphtags["center"] = style;
	style.clear();

	style["halign"] = "justify";
	m_paragraphtags["justify"] = style;
	style.clear();

	style["halign"] = "left";
	m_paragraphtags["left"] = style;
	style.clear();

	style["halign"] = "right";
	m_paragraphtags["right"] = style;
	style.clear();

	m_element = NULL;
	m_paragraph = NULL;

	parse(text);
}

ParsedText::~ParsedText()
{
	for (auto &tag : m_tags)
		delete tag;
}

void ParsedText::parse(const wchar_t *text)
{
	wchar_t c;
	u32 cursor = 0;
	bool escape = false;
	while ((c = text[cursor]) != L'\0') {

		cursor++;

		if (c == L'\r') { // Mac or Windows breaks
			if (text[cursor] == L'\n')
				cursor++;
			// If text has begun, don't skip empty line
			if (m_paragraph) {
				endParagraph();
				enterElement(ELEMENT_SEPARATOR);
			}
			escape = false;
			continue;
		}

		if (c == L'\n') { // Unix breaks
			// If text has begun, don't skip empty line
			if (m_paragraph) {
				endParagraph();
				enterElement(ELEMENT_SEPARATOR);
			}
			escape = false;
			continue;
		}

		if (escape) {
			escape = false;
			pushChar(c);
			continue;
		}

		if (c == L'\\') {
			escape = true;
			continue;
		}

		// Tag check
		if (c == L'<') {
			u32 newcursor = parseTag(text, cursor);
			if (newcursor > 0) {
				cursor = newcursor;
				continue;
			}
		}

		// Default behavior
		pushChar(c);
	}
	endParagraph();
}

void ParsedText::endElement()
{
	m_element = NULL;
}

void ParsedText::endParagraph()
{
	if (!m_paragraph)
		return;

	endElement();
	m_paragraph = NULL;
}

void ParsedText::enterParagraph()
{
	if (!m_paragraph) {
		m_paragraphs.emplace_back();
		m_paragraph = &m_paragraphs.back();
		m_paragraph->setStyle(m_style);
	}
}

void ParsedText::enterElement(ElementType type)
{
	enterParagraph();

	if (!m_element || m_element->type != type) {
		m_paragraph->elements.emplace_back();
		m_element = &m_paragraph->elements.back();
		m_element->type = type;
		m_element->tags = m_active_tags;
		m_element->setStyle(m_style);
	}
}

void ParsedText::pushChar(wchar_t c)
{
	// New word if needed
	if (c == L' ' || c == L'\t')
		enterElement(ELEMENT_SEPARATOR);
	else
		enterElement(ELEMENT_TEXT);

	m_element->text += c;
}

ParsedText::Tag *ParsedText::newTag(std::string name, AttrsList attrs)
{
	endElement();
	Tag *newtag = new Tag();
	newtag->name = name;
	newtag->attrs = attrs;
	m_tags.push_back(newtag);
	return newtag;
}

ParsedText::Tag *ParsedText::openTag(std::string name, AttrsList attrs)
{
	Tag *newtag = newTag(name, attrs);
	m_active_tags.push_front(newtag);
	return newtag;
}

bool ParsedText::closeTag(std::string name)
{
	bool found = false;
	for (auto id = m_active_tags.begin(); id != m_active_tags.end(); ++id)
		if ((*id)->name == name) {
			m_active_tags.erase(id);
			found = true;
			break;
		}
	return found;
}

void ParsedText::globalTag(AttrsList &attrs)
{
	for (auto const& attr : attrs) {
		// Only page level style
		if (attr.first == "margin" && check_integer(attr.second))
			margin = strtol(attr.second.c_str(), NULL, 10);

		if (attr.first == "valign") {
			if (attr.second == "top")
				valign = ParsedText::VALIGN_TOP;
			if (attr.second == "bottom")
				valign = ParsedText::VALIGN_BOTTOM;
			if (attr.second == "middle")
				valign = ParsedText::VALIGN_MIDDLE;
		}

		if (attr.first == "background") {
			irr::video::SColor color;
			if (attr.second == "none")
				background_type = BACKGROUND_NONE;
			else if (parseColorString(attr.second, color, false)) {
				background_type = BACKGROUND_COLOR;
				background_color = color;
			}
		}

		// inheriting style
		if (attr.first == "color" && check_color(attr.second))
			m_root_tag.style["color"] = attr.second;

		if (attr.first == "actioncolor" && check_color(attr.second))
			m_root_tag.style["actioncolor"] = attr.second;

		if (attr.first == "hovercolor" && check_color(attr.second))
			m_root_tag.style["hovercolor"] = attr.second;

		if (attr.first == "size" && strtol(attr.second.c_str(), NULL, 10) > 0)
			m_root_tag.style["fontsize"] = attr.second;

		if (attr.first == "font" &&
				(attr.second == "mono" || attr.second == "normal" ||
				 attr.second == "italic" || attr.second == "bold"))
			m_root_tag.style["fontstyle"] = attr.second;

		if (attr.first == "halign" &&
				(attr.second == "left" || attr.second == "center" ||
						attr.second == "right" || attr.second == "justify"))
			m_root_tag.style["halign"] = attr.second;
	}

}

void ParsedText::parseStyles(AttrsList &attrs, StyleList &style)
{
	if (attrs.count("color") && check_color(attrs["color"]))
		style["color"] = attrs["color"];

	if (attrs.count("hovercolor") && check_color(attrs["hovercolor"]))
		style["hovercolor"] = attrs["hovercolor"];

	if (attrs.count("font") &&
			(attrs["font"] == "mono" || attrs["font"] == "normal" ||
			 attrs["font"] == "bold" || attrs["font"] == "italic"))
		style["fontstyle"] = attrs["font"];

	if (attrs.count("size") && strtol(attrs["size"].c_str(), NULL, 10) > 0)
		style["fontsize"] = attrs["size"];
}

u32 ParsedText::parseTag(const wchar_t *text, u32 cursor)
{
	// Tag name
	bool end = false;
	std::string name = "";
	wchar_t c = text[cursor];

	if (c == L'/') {
		end = true;
		c = text[++cursor];
		if (c == L'\0')
			return 0;
	}

	while (c != ' ' && c != '>') {
		name += c;
		c = text[++cursor];
		if (c == L'\0')
			return 0;
	}

	// Tag attributes
	AttrsList attrs;
	while (c != L'>') {
		std::string attr_name = "";
		std::string attr_val = "";

		while (c == ' ') {
			c = text[++cursor];
			if (c == L'\0' || c == L'=')
				return 0;
		}

		while (c != L' ' && c != L'=') {
			attr_name += (char)c;
			c = text[++cursor];
			if (c == L'\0' || c == L'>')
				return 0;
		}

		while (c == L' ') {
			c = text[++cursor];
			if (c == L'\0' || c == L'>')
				return 0;
		}

		if (c != L'=')
			return 0;

		c = text[++cursor];
		if (c == L'\0')
			return 0;

		while (c != L'>' && c != L' ') {
			attr_val += (char)c;
			c = text[++cursor];
			if (c == L'\0')
				return 0;
		}

		attrs[attr_name] = attr_val;
	}
	++cursor; // Last ">"

	// Tag specific processing
	StyleList style;

	if (name == "global") {
		if (end)
			return 0;
		globalTag(attrs);

	} else if (name == "style") {
		if (end)
			closeTag(name);
		else {
			parseStyles(attrs, style);
			openTag(name, attrs)->style = style;
		}
		endElement();
	} else if (name == "img" || name == "item") {
		if (end)
			return 0;

		// Required attributes
		if (!attrs.count("name"))
			return 0;

		newTag(name, attrs);

		if (name == "img")
			enterElement(ELEMENT_IMAGE);
		else
			enterElement(ELEMENT_ITEM);

		m_element->text = strtostrw(attrs["name"]);

		if (attrs.count("float")) {
			if (attrs["float"] == "left")
				m_element->floating = FLOAT_LEFT;
			if (attrs["float"] == "right")
				m_element->floating = FLOAT_RIGHT;
		}

		if (attrs.count("rotate") && attrs["rotate"] == "yes")
			m_element->rotation = IT_ROT_OTHER;

		if (attrs.count("width")) {
			int width = strtol(attrs["width"].c_str(), NULL, 10);
			if (width > 0)
				m_element->dim.Width = width;
		}

		if (attrs.count("height")) {
			int height = strtol(attrs["height"].c_str(), NULL, 10);
			if (height > 0)
				m_element->dim.Height = height;
		}
		endElement();

	} else if (name == "tag") {
		// Required attributes
		if (!attrs.count("name"))
			return 0;

		StyleList tagstyle;
		parseStyles(attrs, tagstyle);

		if (attrs["paragraph"] == "true")
			m_paragraphtags[attrs["name"]] = tagstyle;
		else
			m_elementtags[attrs["name"]] = tagstyle;

	} else if (name == "action") {
		if (end)
			closeTag(name);
		else {
			if (!attrs.count("name"))
				return 0;
			openTag(name, attrs)->style = m_elementtags["action"];
		}

	} else if (m_elementtags.count(name)) {
		if (end)
			closeTag(name);
		else
			openTag(name, attrs)->style = m_elementtags[name];
		endElement();

	} else if (m_paragraphtags.count(name)) {
		if (end)
			closeTag(name);
		else
			openTag(name, attrs)->style = m_paragraphtags[name];
		endParagraph();

	} else
		return 0; // Unknown tag

	// Update styles accordingly
	m_style.clear();
 	for (auto tag = m_active_tags.crbegin(); tag != m_active_tags.crend(); ++tag)
		for (auto const &prop : (*tag)->style)
			m_style[prop.first] = prop.second;

	return cursor;
}

// -----------------------------------------------------------------------------
// Text Drawer

TextDrawer::TextDrawer(
		const wchar_t* text,
		Client* client,
		gui::IGUIEnvironment* environment,
		ISimpleTextureSource *tsrc) :
		m_text(text), m_client(client), m_environment(environment)
{
	// Size all elements
	for (auto &p : m_text.m_paragraphs) {
		for (auto &e : p.elements) {
			if (e.type == ParsedText::ELEMENT_SEPARATOR ||
					e.type == ParsedText::ELEMENT_TEXT) {
				if (e.font) {
					e.dim.Width = e.font->getDimension(e.text.c_str()).Width;
					e.dim.Height = e.font->getDimension(L"Yy").Height;
				} else
					e.dim = {0, 0};

			} else if (e.type == ParsedText::ELEMENT_IMAGE ||
					e.type == ParsedText::ELEMENT_ITEM) {
				// Dont resize already sized items (sized by another mechanism)
				if (e.dim.Height == 0 || e.dim.Width == 0) {
					// Default image and item size
					core::dimension2d<u32> dim(80, 80);

					if (e.type == ParsedText::ELEMENT_IMAGE) {
						video::ITexture *texture =
							m_client->getTextureSource()->
								getTexture(strwtostr(e.text));
						if (texture)
							dim = texture->getOriginalSize();
					}

					if (e.dim.Height == 0)
						if (e.dim.Width == 0)
							e.dim = dim;
						else
							e.dim.Height = dim.Height * e.dim.Width / dim.Width;
					else
						e.dim.Width = dim.Width * e.dim.Height / dim.Height;
				}
			}
		}
	}
}

// Get element at given coordinates. Coordinates are inner coordinates (starting
// at 0,0).
ParsedText::Element* TextDrawer::getElementAt(core::position2d<s32> pos)
{
	for (auto &p : m_text.m_paragraphs) {
		for (auto &el : p.elements) {
			core::rect<s32> rect(el.pos, el.dim);
			if (rect.isPointInside(pos))
				return &el;
		}
	}
	return 0;
}

void TextDrawer::place(s32 width)
{
	m_floating.clear();
	s32 y = 0;
	s32 ymargin = m_text.margin;

	// Iterator used :
	// p - Current paragraph, walked only once
	// el - Current element, walked only once
	// e and f - local element and floating operators

	for (auto &p : m_text.m_paragraphs) {

		// Find and place floating stuff in paragraph
		for (auto e = p.elements.begin(); e != p.elements.end(); ++e) {
			if (e->floating != ParsedText::FLOAT_NONE) {
				if (y)
					e->pos.Y = y + std::max(ymargin, e->margin);
				else
					e->pos.Y = ymargin;

				if (e->floating == ParsedText::FLOAT_LEFT)
					e->pos.X = m_text.margin;
				if (e->floating == ParsedText::FLOAT_RIGHT)
					e->pos.X = width - e->dim.Width - m_text.margin;

				RectWithMargin floating;
				floating.rect = core::rect<s32>(e->pos, e->dim);
				floating.margin = e->margin;

				m_floating.push_back(floating);
			}
		}

		if (y)
			y = y + std::max(ymargin, p.margin);

		ymargin = p.margin;

		// Place non floating stuff
		std::vector<ParsedText::Element>::iterator el = p.elements.begin();
		while (el != p.elements.end()) {
			// Determine line width and y pos
			s32 left, right;
			s32 nexty = y;
			do {
				y = nexty;
				nexty = 0;

				// Inner left & right
				left = m_text.margin;
				right = width - m_text.margin;

				for (auto f : m_floating) {
					// Is this floating rect interecting paragraph y line ?
					if (f.rect.UpperLeftCorner.Y - f.margin <= y &&
						f.rect.LowerRightCorner.Y + f.margin >= y) {

						// Next Y to try if no room left
						if (!nexty || f.rect.LowerRightCorner.Y +
									std::max(f.margin, p.margin) < nexty)
							nexty = f.rect.LowerRightCorner.Y +
									std::max(f.margin, p.margin) + 1;

						if (f.rect.UpperLeftCorner.X - f.margin <= left &&
							f.rect.LowerRightCorner.X + f.margin < right)
						{ // float on left
							if (f.rect.LowerRightCorner.X +
										std::max(f.margin, p.margin) > left)
								left = f.rect.LowerRightCorner.X +
										std::max(f.margin, p.margin);

						} else if (f.rect.LowerRightCorner.X + f.margin >= right &&
								f.rect.UpperLeftCorner.X - f.margin > left)
						{ // float on right
							if (f.rect.UpperLeftCorner.X -
										std::max(f.margin, p.margin) < right)
								right = f.rect.UpperLeftCorner.X -
										std::max(f.margin, p.margin);

						} else if (f.rect.UpperLeftCorner.X - f.margin <= left &&
							 f.rect.LowerRightCorner.X + f.margin >= right)
						{ // float taking all space
							left = right;
						} else
						{ // float in the middle -- should not occure yet, see that later
						}
					}
				}
			} while (nexty && right <= left);

			u32 linewidth = right - left;
			float x = left;

			u32 charsheight = 0;
			u32 charswidth = 0;
			u32 wordcount = 0;

			// Skip begining of line separators but include them in height
			// computation.
			while(el != p.elements.end() &&
				el->type == ParsedText::ELEMENT_SEPARATOR) {
				if (el->floating == ParsedText::FLOAT_NONE &&
					charsheight < el->dim.Height)
					charsheight = el->dim.Height;
				el++;
			}

			std::vector<ParsedText::Element>::iterator linestart = el;
			std::vector<ParsedText::Element>::iterator lineend = p.elements.end();

			// First pass, find elements fitting into line
			// (or at least one element)
			while(el != p.elements.end() && (charswidth == 0 ||
						charswidth + el->dim.Width <= linewidth)) {
				if (el->floating == ParsedText::FLOAT_NONE) {
					if (el->type != ParsedText::ELEMENT_SEPARATOR) {
						lineend = el;
						wordcount++;
					}
					charswidth += el->dim.Width;
					if (charsheight < el->dim.Height)
						charsheight = el->dim.Height;
				}
				el++;
			}

			// Empty line, nothing to place only go down line height
			if (lineend == p.elements.end()) {
				y += charsheight;
				continue;
			}

			// Point to the first position outside line (may be end())
			lineend++;

			// Second pass, compute printable line width and adjustments
			charswidth = 0;
			for (auto e = linestart; e != lineend; ++e) {
				if (e->floating == ParsedText::FLOAT_NONE)
					charswidth += e->dim.Width;
			}

			float extraspace = 0;

			switch(p.halign) {
			case ParsedText::HALIGN_CENTER:
				x += (linewidth - charswidth) / 2;
				break;
			case ParsedText::HALIGN_JUSTIFY:
				if (wordcount > 1 && // Justification only if at least two words.
					!(lineend == p.elements.end())) // Don't justify last line.
					extraspace = ((float)(linewidth - charswidth)) / (wordcount - 1);
				break;
			case ParsedText::HALIGN_RIGHT:
				x += linewidth - charswidth;
				break;
			case ParsedText::HALIGN_LEFT:
				break;
			}

			// Third pass, actually place everything
			for (auto e = linestart; e != lineend; ++e) {

				if (e->floating != ParsedText::FLOAT_NONE)
					continue;

				e->pos.X = x;
				e->pos.Y = y;

				switch(e->type) {
				case ParsedText::ELEMENT_TEXT:
				case ParsedText::ELEMENT_SEPARATOR:
					e->pos.X = x;

					switch(e->valign) {
					case ParsedText::VALIGN_TOP:
						e->pos.Y = y;
						break;
					case ParsedText::VALIGN_MIDDLE:
						e->pos.Y = y + (charsheight - e->dim.Height) / 2 ;
						break;
					case ParsedText::VALIGN_BOTTOM:
					default:
						e->pos.Y = y + charsheight - e->dim.Height ;
					}

					x += e->dim.Width;
					if (e->type == ParsedText::ELEMENT_SEPARATOR)
						x += extraspace;
					break;

				case ParsedText::ELEMENT_IMAGE:
				case ParsedText::ELEMENT_ITEM:
					x += e->dim.Width;
					break;
				}
			}
			y += charsheight;

		} // Elements (actually lines)
	} // Paragraph

	// Check if float goes under paragraph
	for (auto f : m_floating) {
		if (f.rect.LowerRightCorner.Y >= y)
			y = f.rect.LowerRightCorner.Y;
	}

	m_height = y + m_text.margin;
}

// Get vertical offset according to valign
s32 TextDrawer::getVoffset(s32 height)
{
	switch(m_text.valign) {
	case ParsedText::VALIGN_BOTTOM:
		return height - m_height;
	case ParsedText::VALIGN_MIDDLE:
		return (height - m_height) / 2;
	case ParsedText::VALIGN_TOP:
	default:
		return 0;
	}
}

// Draw text in a rectangle with a given offset. Items are actually placed in
// relative (to upper left corner) coordinates.
void TextDrawer::draw(
	core::rect<s32> dest_rect,
	core::position2d<s32> dest_offset)
{
	core::position2d<s32> offset = dest_rect.UpperLeftCorner + dest_offset;

	if (m_text.background_type == ParsedText::BACKGROUND_COLOR)
		m_environment->getVideoDriver()->draw2DRectangle(
				m_text.background_color, dest_rect);

	for (auto &p : m_text.m_paragraphs) {
		for (auto &el : p.elements) {
			core::rect<s32> rect(el.pos + offset, el.dim);
			if (rect.isRectCollided(dest_rect)) {

				switch(el.type) {
				case ParsedText::ELEMENT_TEXT: {
					irr::video::SColor color = el.color;

					for (auto tag : el.tags)
						if (&(*tag) == m_hovertag)
							color = el.hovercolor;

					if (el.font)
						el.font->draw(el.text, rect, color, false, true, &dest_rect);
				} break;

				case ParsedText::ELEMENT_SEPARATOR:
					break;

				case ParsedText::ELEMENT_IMAGE: {
					video::ITexture *texture =
						m_client->getTextureSource()->getTexture(
									strwtostr(el.text));
					if (texture != 0)
						m_environment->getVideoDriver()->draw2DImage(
								texture, rect,
								irr::core::rect<s32>(
										core::position2d<s32>(0, 0),
										texture->getOriginalSize()),
								&dest_rect, 0, true);
				} break;

				case ParsedText::ELEMENT_ITEM: {
					IItemDefManager *idef = m_client->idef();
					ItemStack item;
					item.deSerialize(strwtostr(el.text), idef);

					drawItemStack(
							m_environment->getVideoDriver(),
							g_fontengine->getFont(), item, rect, &dest_rect,
							m_client, el.rotation);
				} break;
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------
// GUIHyperText - The formated text area formspec item

//! constructor
GUIHyperText::GUIHyperText(
	const wchar_t* text,
	IGUIEnvironment* environment,
	IGUIElement* parent, s32 id,
	const core::rect<s32>& rectangle,
	Client *client,
	ISimpleTextureSource *tsrc) :
	IGUIElement(EGUIET_ELEMENT, environment, parent, id, rectangle),
	m_client(client), m_vscrollbar(nullptr),
	m_drawer(text, client, environment, tsrc), m_text_scrollpos(0, 0)
{
#ifdef _DEBUG
	setDebugName("GUIHyperText");
#endif

	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();

	m_scrollbar_width = skin ? skin->getSize(gui::EGDS_SCROLLBAR_SIZE) : 16;

	core::rect<s32> rect = irr::core::rect<s32>(
			RelativeRect.getWidth() - m_scrollbar_width, 0,
			RelativeRect.getWidth(), RelativeRect.getHeight());

	m_vscrollbar = Environment->addScrollBar(false, rect, this, -1);
	m_vscrollbar->setVisible(false);
}

//! destructor
GUIHyperText::~GUIHyperText()
{
	m_vscrollbar->remove();
}

ParsedText::Element *GUIHyperText::getElementAt(s32 X, s32 Y)
{
	core::position2d<s32> pos = {X, Y};
	pos -= m_display_text_rect.UpperLeftCorner;
	pos -= m_text_scrollpos;
	return m_drawer.getElementAt(pos);
}

void GUIHyperText::checkHover(s32 X, s32 Y)
{
	m_drawer.m_hovertag = nullptr;

	if (AbsoluteRect.isPointInside(core::position2d<s32>(X, Y)))
	{
		ParsedText::Element *element = getElementAt(X, Y);

		if (element)
			for (auto &tag : element->tags)
				if (tag->name == "action") {
					m_drawer.m_hovertag = tag;
					break;
				}
	}

	if (m_drawer.m_hovertag)
		RenderingEngine::get_raw_device()->getCursorControl()->
			setActiveIcon(gui::ECI_HAND);
	else
		RenderingEngine::get_raw_device()->getCursorControl()->
			setActiveIcon(gui::ECI_NORMAL);
}

bool GUIHyperText::OnEvent(const SEvent &event) {
	// Scroll bar
	if (event.EventType == EET_GUI_EVENT &&
		event.GUIEvent.EventType == EGET_SCROLL_BAR_CHANGED &&
		event.GUIEvent.Caller == m_vscrollbar) {
		m_text_scrollpos.Y = -m_vscrollbar->getPos();
	}

	// Reset hover if element left
	if (event.EventType == EET_GUI_EVENT &&
		event.GUIEvent.EventType == EGET_ELEMENT_LEFT) {
		m_drawer.m_hovertag = nullptr;
		RenderingEngine::get_raw_device()->getCursorControl()->
			setActiveIcon(gui::ECI_NORMAL);
	}

	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		if (event.MouseInput.Event == EMIE_MOUSE_MOVED) {
			if (AbsoluteRect.isPointInside(core::position2d<s32>(
					event.MouseInput.X, event.MouseInput.Y)))
				checkHover(event.MouseInput.X, event.MouseInput.Y);
		}

		if (event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
			m_vscrollbar->setPos(m_vscrollbar->getPos() -
					event.MouseInput.Wheel * m_vscrollbar->getSmallStep());
			m_text_scrollpos.Y = -m_vscrollbar->getPos();
			m_drawer.draw(m_display_text_rect, m_text_scrollpos);
			checkHover(event.MouseInput.X,event.MouseInput.Y);

		} else if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
			ParsedText::Element* element =
				getElementAt(event.MouseInput.X, event.MouseInput.Y);

			if (element) {
				for (auto & tag : element->tags) {
					if (tag->name == "action") {
						Text = core::stringw(L"action:") +
								strtostrw(tag->attrs["name"]);
						if (Parent) {
							SEvent newEvent;
							newEvent.EventType = EET_GUI_EVENT;
							newEvent.GUIEvent.Caller = this;
							newEvent.GUIEvent.Element = 0;
							newEvent.GUIEvent.EventType = EGET_BUTTON_CLICKED;
							Parent->OnEvent(newEvent);
						}
						break;
					}
				}
			}
		}
	}

	return IGUIElement::OnEvent(event);
}

//! draws the element and its children
void GUIHyperText::draw()
{
	if (!IsVisible)
		return;

	// Text
	m_display_text_rect = AbsoluteRect;

	m_drawer.place(m_display_text_rect.getWidth());

	if (m_drawer.getHeight() > m_display_text_rect.getHeight()) {

		m_vscrollbar->setSmallStep(m_display_text_rect.getWidth() * 0.1f);
		m_vscrollbar->setLargeStep(m_display_text_rect.getWidth() * 0.5f);

		m_vscrollbar->setMax(m_drawer.getHeight() -
				m_display_text_rect.getHeight());

		m_vscrollbar->setVisible(true);

		core::rect<s32> smaller_rect = m_display_text_rect;
		smaller_rect.LowerRightCorner.X -= m_scrollbar_width;
		m_drawer.place(smaller_rect.getWidth());
		m_drawer.draw(m_display_text_rect, m_text_scrollpos);

	} else {
		core::position2d<s32> offset = {
				0, m_drawer.getVoffset(m_display_text_rect.getHeight())};

		m_drawer.draw(m_display_text_rect, offset);
		m_vscrollbar->setVisible(false);
	}

	// draw children
	IGUIElement::draw();
}
