
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include <vector>
#include <unordered_map>
using namespace irr::gui;
#include "guiText.h"
#include "fontengine.h"
#include <SColor.h>


// PARSER ?
/*
struct currentstate {
	int halign = left;
	int fontsize = medium;

}

struct inline_content {
	core::dimension2d<u32> dimension;
	core::stringw text;
	gui::IGUIFont* font;
}

struct inline_block {
	core::dimension2d<u32> dimension;
	std::vector<GUIText:inline_content>& content;
};

std::vector<GUIText::inline_block>& inline_blocks;
*/

void GUIText::update_properties()
{
	m_style_properties.clear();
	for (auto const& tag : m_tag_stack)
		for (auto const& prop : tag.style_properties)
			m_style_properties[prop.first] = prop.second;

/*	printf("Tag stack:");
	for (auto const& tag: m_tag_stack)
		printf("%ls", tag.name.c_str());
	printf("\nProperties:\n");
	for (auto const& prop : m_style_properties)
		printf("  %s='%s'\n", prop.first.c_str(), prop.second.c_str());
*/}

void GUIText::end_paragraph()
{
	printf("\n(paragraph end)\n");
}

void GUIText::push_char(wchar_t c)
{
	printf("%lc", c);
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
			properties["fontsize"] = "12";
			tag_start = true;
		}
	} else if (tag_name == L"medium") {
		if (!tag_end) {
			properties["fontsize"] = "16";
			tag_start = true;
		}
	} else if (tag_name == L"large") {
		if (!tag_end) {
			properties["fontsize"] = "22";
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
	} else return 0; // Unknown tag

	if (tag_end)
		printf("\ntag '%ls' end\n", tag_name.c_str());
	else
		printf("\ntag '%ls'\n", tag_name.c_str());

	if (tag_start) {
		markup_tag tag;
		tag.name = tag_name;
		tag.style_properties = properties;

		m_tag_stack.push_back(tag);
		update_properties();
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
		update_properties();
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
	root_tag.style_properties["color"] = "#FFFFFF";
	m_tag_stack.push_back(root_tag);

	update_properties();

//	inline_blocks.clear()
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
			continue;
		}

		if (c == L'\n') { // Unix breaks
			end_paragraph();
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
	parse();
}

//! destructor
GUIText::~GUIText()
{}

void GUIText::drawline(
	core::rect<s32>& rectangle,
	std::vector<GUIText::word>& words,
	int halign)
{

	s32 textwidth = 0;
	s32 textheight = 0;
	float x = 0;
	float extraspace = 0;

	for (auto & word : words) {
		textwidth += word.dimension.Width;
		if (textheight < word.dimension.Height)
			textheight = word.dimension.Height;
	}

	rectangle.LowerRightCorner.Y = rectangle.UpperLeftCorner.Y + textheight;

	switch(halign)
	{
		case center:
			x = (rectangle.getWidth() - textwidth) / 2;
			break;
		case justify:
			if (words.size() > 1)
				extraspace = ((float)(rectangle.getWidth() - textwidth)) / (words.size() - 1);
			break;
		case right:
			x = (rectangle.getWidth() - textwidth);
	}

	for (auto & word : words) {
		core::rect<s32> c = rectangle + core::position2d<s32>(x, 0);
		if (c.isRectCollided(AbsoluteClippingRect)) {
			word.font->draw(word.text, c,
				irr::video::SColor(255, 255, 255, 255),
				false, true, &AbsoluteClippingRect);
		}
		x += word.dimension.Width + extraspace;
	}
	words.clear();
	rectangle += core::position2d<s32>(0, textheight);
}

//! draws the element and its children
void GUIText::draw()
{
	if (!IsVisible)
		return;

	IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;
//	IGUIFont* font = skin->getFont();

//	skin->draw2DRectangle (this, irr::video::SColor(255, 255, 0, 0), AbsoluteRect);
//	skin->draw2DRectangle (this, irr::video::SColor(255, 0, 255, 0), AbsoluteClippingRect);

	IGUIFont* font = g_fontengine->getFont(18);
	if (!font)
		return;

	s32 linewidth = AbsoluteRect.getWidth();

	std::vector<core::stringw> paragraphs;
	Text.split(paragraphs, L"\n");

	core::rect<s32> line_rect = AbsoluteRect;
	std::vector<GUIText::word> words_to_draw;

	for (auto & paragraph : paragraphs) {
		std::vector<core::stringw> words;

		paragraph.split(words, L" ", 1, false, true);

		u32 width = 0;

		for (auto & word : words)
		{
			if (font->getDimension(word.c_str()).Width + width > linewidth)
			{
				// TODO: Split very large words
				drawline(line_rect, words_to_draw, justify);
				width = 0;
			}
			// Remove first space of each line
			if (words_to_draw.size() == 0 && (word[0] == L' ')) word.erase(0);

			GUIText::word word_to_draw(font->getDimension(word.c_str()), word, font);
			words_to_draw.push_back(word_to_draw);
			width += word_to_draw.dimension.Width;
		}

		if (! words_to_draw.empty())
			drawline(line_rect, words_to_draw, left);
	}
	// draw children
	IGUIElement::draw();
}
