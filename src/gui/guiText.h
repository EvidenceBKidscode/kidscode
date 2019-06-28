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


		void update_properties();
		void end_paragraph();
		void push_char(wchar_t c);
		u32 parse_tag(u32 cursor);
		void parse();

		struct markup_tag {
			core::stringw name;
			std::unordered_map<std::string, std::string> style_properties;
		};

		std::vector<GUIText::markup_tag> m_tag_stack;
		std::unordered_map<std::string, std::string> m_style_properties;

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
