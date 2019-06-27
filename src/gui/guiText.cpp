
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include <vector>
using namespace irr::gui;
#include "guiText.h"

#include <SColor.h>

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

void GUIText::drawline(
	core::rect<s32>& rectangle,
	std::vector<GUIText::word>& words,
	int halign)
{
	core::rect<s32> word_rect = rectangle;

	s32 textwidth = 0;
	s32 textheight = 0;
	s32 startx = 0;
	s32 extraspace = 0;

	for (auto & word : words) {
		textwidth += word.dimension.Width;
		if (textheight < word.dimension.Height)
			textheight = word.dimension.Height;
	}

	switch(halign)
	{
		case center:
			startx = (rectangle.getWidth() - textwidth) / 2;
			break;
		case justify:
			if (words.size() > 1)
				extraspace = (rectangle.getWidth() - textwidth) / (words.size() - 1);
			break;
		case right:
			startx = (rectangle.getWidth() - textwidth);
	}

	word_rect += core::position2d<s32>(startx, 0);

	for (auto & word : words) {
		word.font->draw(word.text, word_rect,
			irr::video::SColor(255, 255, 255, 255),
			false, true, 0); //&local_clip_rect);
		word_rect += core::position2d<s32>(word.dimension.Width + extraspace, 0);
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
	IGUIFont* font = skin->getFont();
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
			// Remove first space of each line
			if (words_to_draw.size() == 0 && word[0] == L' ') word.erase(0);

			GUIText::word word_to_draw(font->getDimension(word.c_str()), word, font);

			if (word_to_draw.dimension.Width + width <= linewidth)
			{
				words_to_draw.push_back(word_to_draw);
				width += word_to_draw.dimension.Width;
			} else {
				drawline(line_rect, words_to_draw, justify);
				width = 0;
			}
		}

		if (! words_to_draw.empty())
			drawline(line_rect, words_to_draw, left);
	}
	// draw children
	IGUIElement::draw();
}
