
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include <vector>
#include <unordered_map>
using namespace irr::gui;
#include "guiText.h"
#include "fontengine.h"
#include <SColor.h>

bool GUIText::update_style()
{
	GUIText::properties style_properties;
	for (auto const& tag : m_tag_stack)
		for (auto const& prop : tag.style_properties)
			style_properties[prop.first] = prop.second;

	if (style_properties["halign"] == "center")
		m_style.halign = center;
	else if (style_properties["halign"] == "right")
		m_style.halign = right;
	else if (style_properties["halign"] == "justify")
		m_style.halign = justify;
	else
		m_style.halign = left;

	int r, g, b;
	sscanf(style_properties["color"].c_str(),"%2x%2x%2x", &r, &g, &b);
	m_style.color = irr::video::SColor(255, r, g, b);

	unsigned int font_size = std::atoi(style_properties["fontsize"].c_str());
	FontMode font_mode = FM_Standard;
	if (style_properties["fontstyle"] == "mono")
		font_mode = FM_Mono;
	m_style.font = g_fontengine->getFont(font_size, font_mode);

	if (!m_style.font) {
		printf("No font found ! Size=%d, mode=%d\n", font_size, font_mode);
		return false;
	}

	return true;
}

void GUIText::draw_line(bool lastline)
{
	s32 textwidth = 0;
	s32 textheight = 0;
	float x = 0;
	float extraspace = 0;

	// remove trailing non printable chars
	for (auto word = m_current_paragraph.words.rbegin();
			word != m_current_paragraph.words.rend(); ++word)
		if (word->separator)
			m_current_paragraph.words.erase(std::next(word).base());
		else
			break;

	for (auto & word : m_current_paragraph.words) {
		textwidth += word.dimension.Width;
		if (textheight < word.dimension.Height)
			textheight = word.dimension.Height;
	}

	switch(m_current_paragraph.style.halign)
	{
		case center:
			x = (m_current_paragraph.linewidth - textwidth) / 2;
			break;
		case justify:
			if (!lastline && m_current_paragraph.words.size() > 1)
				extraspace = ((float)(m_current_paragraph.linewidth - textwidth)) /
						(m_current_paragraph.words.size() - 1);
			break;
		case right:
			x = m_current_paragraph.linewidth - textwidth;
	}

	for (auto & word : m_current_paragraph.words) {
		for (auto & fragment : word.fragments) {
			core::rect<s32> c(
				m_current_paragraph.pos.X + x,
				m_current_paragraph.pos.Y,
				m_current_paragraph.pos.X + x + fragment.dimension.Width,
				m_current_paragraph.pos.Y + textheight);

			if (c.isRectCollided(AbsoluteClippingRect)) {
				fragment.style.font->draw(fragment.text, c,
					fragment.style.color,
					false, true, &AbsoluteClippingRect);
			}
			x += fragment.dimension.Width;
		}
		x += extraspace;
	}

	m_current_paragraph.pos.Y += textheight;
	m_current_paragraph.width = 0;
	m_current_paragraph.words.clear();
}

void GUIText::push_char(wchar_t c)
{
	bool separator = (c == L' ' || c == L'\t');

		// New paragraph if needed
	if (m_current_paragraph.ended) {
		m_current_paragraph.ended = false;
		m_current_paragraph.style = m_style;
		m_current_paragraph.width = 0;
		m_current_paragraph.words.clear(); // Should already be cleared
	};

	if (m_current_word.separator != separator)
		end_word();

	// New word if needed
	if (m_current_word.ended) {
		m_current_word.ended = false;
		m_current_word.fragments.clear();
		m_current_word.separator = separator;
	};


	// New fragement if needed
	if (m_current_fragment.ended) {
		m_current_fragment.ended = false;
		m_current_fragment.style = m_style;
		m_current_fragment.text = "";
	}

	m_current_fragment.text += c;
}

void GUIText::end_fragment()
{
	if (m_current_fragment.ended) return;

	// Compute size
	m_current_fragment.dimension =
			m_current_fragment.style.font->getDimension(
					m_current_fragment.text.c_str());

	m_current_fragment.ended = true;
	m_current_word.fragments.push_back(m_current_fragment);
}

void GUIText::end_word()
{
	if (m_current_word.ended) return;

	end_fragment();

	// Compute size
	m_current_word.dimension.Height = 0;
	m_current_word.dimension.Width = 0;

	for (auto const & fragment: m_current_word.fragments) {
		if (m_current_word.dimension.Height < fragment.dimension.Height)
			m_current_word.dimension.Height = fragment.dimension.Height;
		m_current_word.dimension.Width += fragment.dimension.Width;
	}

	if (m_current_paragraph.width + m_current_word.dimension.Width >
			m_current_paragraph.linewidth)
		draw_line(false);

	m_current_word.ended = true;

	if (!m_current_word.separator || !m_current_paragraph.words.empty()) {
		m_current_paragraph.words.push_back(m_current_word);
		m_current_paragraph.width += m_current_word.dimension.Width;
	}
}

void GUIText::end_paragraph()
{
	if (m_current_paragraph.ended) return;

	end_word();

	if (!m_current_paragraph.words.empty())
		draw_line(true);
}

char getwcharhexdigit(wchar_t c)
{
	if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) return (char)c;
	if (c >= 'a' && c <= 'f') return (char)(c - 32);
	return 0;
}

u32 GUIText::parse_tag(u32 cursor)
{
	u32 textsize = Text.size();
	bool tag_end = false;
	bool tag_start = false;
	core::stringw tag_name = "";
	core::stringw tag_param = "";
	wchar_t c;

	if (cursor >= textsize) return 0;
	c = Text[cursor];

	if (c == L'/') {
		tag_end = true;
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	while (c != ' ' && c != '>')
	{
		tag_name += c;
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	while (c == ' ') {
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	while (c != '>') {
		tag_param += c;
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	++cursor; // last '>'

	std::unordered_map<std::string, std::string> properties;

	if (tag_name == L"center") {
		if (!tag_end) {
			properties["halign"] = "center";
			tag_start = true;
		}
	} else if (tag_name == L"justify") {
		if (!tag_end) {
			properties["halign"] = "justify";
			tag_start = true;
		}
	} else if (tag_name == L"left") {
		if (!tag_end) {
			properties["halign"] = "left";
			tag_start = true;
		}
	} else if (tag_name == L"right") {
		if (!tag_end) {
			properties["halign"] = "right";
			tag_start = true;
		}
	} else if (tag_name == L"small") {
		if (!tag_end) {
			properties["fontsize"] = "16";
			tag_start = true;
		}
	} else if (tag_name == L"medium") {
		if (!tag_end) {
			properties["fontsize"] = "24";
			tag_start = true;
		}
	} else if (tag_name == L"large") {
		if (!tag_end) {
			properties["fontsize"] = "36";
			tag_start = true;
		}
	} else if (tag_name == L"mono") {
		if (!tag_end) {
			properties["fontstyle"] = "mono";
			tag_start = true;
		}
	} else if (tag_name == L"normal") {
		if (!tag_end) {
			properties["fontstyle"] = "normal";
			tag_start = true;
		}
	} else if (tag_name == L"color") {
		if (!tag_end) {
			if (tag_param[0] == L'#') {
				std::string color = "";
				char c;
				if (tag_param.size() == 4) {
					if (!(c = getwcharhexdigit(tag_param[1]))) return 0;
					color += c; color += c;
					if (!(c = getwcharhexdigit(tag_param[2]))) return 0;
					color += c; color += c;
					if (!(c = getwcharhexdigit(tag_param[3]))) return 0;
					color += c; color += c;
				} else if (tag_param.size() == 7) {
					if (!(c = getwcharhexdigit(tag_param[1]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[2]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[3]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[4]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[5]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[6]))) return 0;
					color += c;
				} else return 0;
				properties["color"] = color;
				tag_start = true;
			} else return 0;
		}
	} else return 0; // Unknown tag

	if (tag_start) {
		markup_tag tag;
		tag.name = tag_name;
		tag.style_properties = properties;

		m_tag_stack.push_back(tag);
		update_style();
	}

	// Tag end, unstack last stacked tag with same name
	if (tag_end) {
		bool found = false;
		for (auto tag = m_tag_stack.rbegin(); tag != m_tag_stack.rend(); ++tag)
			if (tag->name == tag_name) {
				m_tag_stack.erase(std::next(tag).base());
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
	m_tag_stack.clear();
	markup_tag root_tag;
	root_tag.name = L"root";
	root_tag.style_properties["fontsize"] = "12";
	root_tag.style_properties["fontstyle"] = "normal";
	root_tag.style_properties["halign"] = "left";
	root_tag.style_properties["color"] = "FFFFFF";
	m_tag_stack.push_back(root_tag);
	update_style();

	m_current_fragment.ended = true;
	m_current_word.ended = true;
	m_current_paragraph.ended = true;

	u32 cursor = 0;
	bool escape = false;
	wchar_t c;
	u32 textsize = Text.size();
	while (cursor < textsize)
	{
		c = Text[cursor];
		cursor++;

		if (c == L'\r') { // Mac or Windows breaks
			if (cursor < textsize && Text[cursor] == L'\n')
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
}

//! constructor
GUIText::GUIText(
	const wchar_t* text,
	IGUIEnvironment* environment,
	IGUIElement* parent, s32 id,
	const core::rect<s32>& rectangle)
	: IGUIElement(EGUIET_ELEMENT, environment, parent, id, rectangle)
{
	#ifdef _DEBUG
		setDebugName("GUIText");
	#endif

	Text = text;
}

//! destructor
GUIText::~GUIText()
{}
//! draws the element and its children
void GUIText::draw()
{
	if (!IsVisible)
		return;

	m_current_paragraph.linewidth = AbsoluteRect.getWidth();
	m_current_paragraph.pos = AbsoluteRect.UpperLeftCorner;

	parse();

	// draw children
	IGUIElement::draw();
}
