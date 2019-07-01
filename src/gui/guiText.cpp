
#include "IGUIEnvironment.h"
#include "guiScrollBar.h"
#include "IGUIFont.h"
#include <vector>
#include <unordered_map>
using namespace irr::gui;
#include "guiText.h"
#include "fontengine.h"
#include <SColor.h>

//! constructor
GUIText::GUIText(
	const wchar_t* text,
	IGUIEnvironment* environment,
	IGUIElement* parent, s32 id,
	const core::rect<s32>& rectangle) :
	IGUIElement(EGUIET_ELEMENT, environment, parent, id, rectangle),
	m_vscrollbar(NULL),
	m_text_scrollpos(0, 0)
{
	#ifdef _DEBUG
		setDebugName("GUIText");
	#endif

	Text = text;

	createVScrollBar();
}

//! destructor
GUIText::~GUIText()
{
	m_vscrollbar->remove();
}

//! create a vertical scroll bar
void GUIText::createVScrollBar()
{
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

bool GUIText::OnEvent(const SEvent& event) {
	if (event.EventType == EET_GUI_EVENT)
		if (event.GUIEvent.EventType == EGET_SCROLL_BAR_CHANGED)
			if (event.GUIEvent.Caller == m_vscrollbar)
				m_text_scrollpos.Y = -m_vscrollbar->getPos();

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
	parse();
	size(m_parsed_text);
	place(m_parsed_text, m_display_text_rect, m_text_scrollpos);

	if (m_parsed_text.height > m_display_text_rect.getHeight()) {

		m_vscrollbar->setMax(m_parsed_text.height - m_display_text_rect.getHeight());
		m_vscrollbar->setVisible(true);

		core::rect<s32> smaller_rect = m_display_text_rect;
		smaller_rect.LowerRightCorner.X -= m_scrollbar_width;
		place(m_parsed_text, smaller_rect, m_text_scrollpos);

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

void GUIText::draw(GUIText::text &text, core::rect<s32> & text_rect)
{
	for (auto & paragraph : text.paragraphs)
		for (auto & word : paragraph.words)
			if (word.draw)
				for (auto & fragment : word.fragments)
					if (fragment.draw)
					{
						core::rect<s32> c(
							fragment.position.X, fragment.position.Y,
							fragment.position.X + fragment.dimension.Width,
							fragment.position.Y + fragment.dimension.Height);

						fragment.style.font->draw(fragment.text, c,
							fragment.style.color, false, true, &text_rect);
					}
}

// -----------------------------------------------------------------------------
// Placing
// -----------------------------------------------------------------------------

void GUIText::size(GUIText::text &text)
{
	for (auto & paragraph : text.paragraphs) {
		for (auto & word : paragraph.words) {
			word.dimension.Height = 0;
			word.dimension.Width = 0;

			for (auto & fragment : word.fragments) {
				fragment.dimension =
					fragment.style.font->getDimension(fragment.text.c_str());
				if (word.dimension.Height < fragment.dimension.Height)
					word.dimension.Height = fragment.dimension.Height;
				word.dimension.Width += fragment.dimension.Width;
			}
		}
	}
}

void GUIText::place(
	GUIText::text & text,
	core::rect<s32> & text_rect,
	core::position2d<s32> & offset)
{
	text.height = 0;
	core::position2d<s32> position = text_rect.UpperLeftCorner + offset;
	for (auto & paragraph : text.paragraphs) {
		paragraph.height = 0;

		std::vector<GUIText::word>::iterator current_word =  paragraph.words.begin();
		while (current_word != paragraph.words.end())
	 	{
			s32 textwidth = 0;
			s32 textheight = 0;
			u32 wordcount = 0;
			u32 linewidth = text_rect.getWidth();

			std::vector<GUIText::word>::iterator linestart = current_word;

			while(current_word != paragraph.words.end() && current_word->separator) {
				current_word->draw = false;
				current_word++;
			}

			// Need a distinct value to detect empty lines
			std::vector<GUIText::word>::iterator lineend = paragraph.words.end();

			// First pass, find words fitting into line
			while(current_word != paragraph.words.end() &&
					textwidth + current_word->dimension.Width <= linewidth) {

				if (! current_word->separator) {
					lineend = current_word;
					wordcount++;
				}

				textwidth += current_word->dimension.Width;
				current_word++;
			}

			// Empty line, nothing to place or draw
			if (lineend == paragraph.words.end()) return;

			lineend++; // Point to the first position outside line (may be end())
			bool lastline = lineend == paragraph.words.end();

			// Second pass, compute printable line width and adjustments
			textwidth = 0;
			for(auto word = linestart; word != lineend; ++word) {
				textwidth += word->dimension.Width;
				if (textheight < word->dimension.Height)
					textheight = word->dimension.Height;
			}

			float x = 0;
			float extraspace = 0;

			switch(paragraph.style.halign)
			{
				case center:
					x = (linewidth - textwidth) / 2;
					break;
				case justify:
					if (wordcount > 1 && !lastline)
						extraspace = ((float)(linewidth - textwidth)) / (wordcount - 1);
					break;
				case right:
					x = linewidth - textwidth;
			}

			// Third pass, actually place everything
			for(auto word = linestart; word != lineend; ++word)
			{
				for (auto fragment = word->fragments.begin();
					fragment != word->fragments.end(); ++fragment)
				{
					fragment->position.X = position.X + x;
					fragment->position.Y = position.Y; // TODO: valing mnagement
					core::rect<s32> c(fragment->position.X, fragment->position.Y,
						fragment->position.X + fragment->dimension.Width,
						fragment->position.Y + fragment->dimension.Height);

					fragment->draw = c.isRectCollided(text_rect);
					word->draw = word->draw || fragment->draw;
					if (fragment->draw)
						x += fragment->dimension.Width;
				}
				if (word->separator)
					x += extraspace;
			}
			paragraph.height += textheight;
			position.Y += textheight;
		}
		text.height += paragraph.height;

		// TODO: Paragraph spacing to be set in stye
		// TODO: margin managment (overlapping)
		text.height += 10;
		position.Y += 10;
	}
}

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------

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

void GUIText::end_fragment()
{
	if (m_current_fragment.ended) return;
	m_current_fragment.ended = true;
	m_current_word.fragments.push_back(m_current_fragment);
}

void GUIText::end_word()
{
	if (m_current_word.ended) return;

	end_fragment();
	m_current_word.ended = true;
	m_current_paragraph.words.push_back(m_current_word);
}

void GUIText::end_paragraph()
{
	if (m_current_paragraph.ended) return;

	end_word();

	m_current_paragraph.ended = true;

	m_parsed_text.paragraphs.push_back(m_current_paragraph);
}

void GUIText::push_char(wchar_t c)
{
	bool separator = (c == L' ' || c == L'\t');

		// New paragraph if needed
	if (m_current_paragraph.ended) {
		m_current_paragraph.ended = false;
		m_current_paragraph.style = m_style;
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
		end_paragraph();
	} else if (tag_name == L"justify") {
		if (!tag_end) {
			properties["halign"] = "justify";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag_name == L"left") {
		if (!tag_end) {
			properties["halign"] = "left";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag_name == L"right") {
		if (!tag_end) {
			properties["halign"] = "right";
			tag_start = true;
		}
		end_paragraph();
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
	m_parsed_text.paragraphs.clear();

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
	end_paragraph();
}
