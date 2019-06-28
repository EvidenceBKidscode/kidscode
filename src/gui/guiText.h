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

		bool OnEvent(const SEvent& event);

	protected:

		core::rect<s32> m_display_text_rect;

		core::position2d<s32> m_text_scrollpos;

		u32 m_scrollbar_width;
		GUIScrollBar *m_vscrollbar;

		void createVScrollBar();

		enum halign { center, left, right, justify };

		typedef std::unordered_map<std::string, std::string> properties;

		struct markup_tag {
			core::stringw name;
			GUIText::properties style_properties;
		};

		struct style_def {
			int halign;
			gui::IGUIFont* font;
			irr::video::SColor color;
		};

		std::vector<GUIText::markup_tag> m_tag_stack;
		style_def m_style;

		bool update_style();
		void draw_line(bool lastline);
		void end_fragment();
		void end_word();
		void end_paragraph();
		void push_char(wchar_t c);
		u32 parse_tag(u32 cursor);
		void parse();

		struct fragment {
			style_def style;
			core::stringw text;
			core::dimension2d<u32> dimension;
			bool ended = false;
		};

		struct word {
			std::vector<fragment> fragments;
			core::dimension2d<u32> dimension;
			bool ended = false;
			bool separator = false;
		};

		struct paragraph {
			style_def style;
			std::vector<word> words;
			u32 width = 0;
			u32 linewidth;
			core::position2d<s32> pos;
			bool ended = false;
		};

		fragment m_current_fragment;
		word m_current_word;
		paragraph m_current_paragraph;
};


#endif // GUITEXT_HEADER
