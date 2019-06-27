
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include <vector>
using namespace irr::gui;
#include "guiText.h"
#include "fontengine.h"
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
