#ifndef GUITEXT_HEADER
#define GUITEXT_HEADER

#include "IGUIElement.h"

using namespace irr;
using namespace irr::gui;

class GUIText : public IGUIElement
{
	public:

		//! constructor
		GUIText(const wchar_t* text, IGUIEnvironment* environment, IGUIElement* parent, s32 id, const core::rect<s32>& rectangle);

		//! destructor
		virtual ~GUIText();

		//! draws the element and its children
		virtual void draw();

	protected:
};


#endif // GUITEXT_HEADER
