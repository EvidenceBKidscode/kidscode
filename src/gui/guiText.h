#ifndef GUITEXT_HEADER
#define GUITEXT_HEADER

#include "IGUIElement.h"

using namespace irr;

class GUIText : public gui::IGUIElement
{
	public:

		//! constructor
		GUIText(const wchar_t* text, gui::IGUIEnvironment* environment, gui::IGUIElement* parent, s32 id, const core::rect<s32>& rectangle);

		//! destructor
		virtual ~GUIText();

		//! draws the element and its children
		virtual void draw();

	protected:

		enum halign {
			center,
			left,
			right,
			justify
		};

		struct word {
			core::dimension2d<u32> dimension;
			core::stringw text;
			gui::IGUIFont* font;
			word(core::dimension2d<u32> dim, core::stringw txt, gui::IGUIFont* fnt):
				dimension(dim), text(txt)
				{ font = fnt; }
		};

		void drawline(
			core::rect<s32>& rectangle,
			std::vector<GUIText::word>& words,
			int halign);
};


#endif // GUITEXT_HEADER
