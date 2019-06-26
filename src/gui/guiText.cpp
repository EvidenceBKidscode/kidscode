#include "guiText.h"
#include "IGUIFont.h"
#include "IGUIEnvironment.h"
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

	font->draw(Text, AbsoluteRect, //m_current_text_rect,
		irr::video::SColor(255, 255, 255, 255),
		false, true, 0); //&local_clip_rect);

	// draw children
	IGUIElement::draw();
}
