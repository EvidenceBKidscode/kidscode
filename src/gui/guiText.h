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
		struct fragment {
			style_def style;
			core::stringw text;
			core::dimension2d<u32> dimension;
			core::position2d<s32> position;
			bool draw = false;
			bool ended = false;
		};

		struct word {
			std::vector<fragment> fragments;
			core::dimension2d<u32> dimension;
			bool ended = false;
			bool draw = false;
			bool separator = false;
		};

		struct paragraph {
			style_def style;
			std::vector<word> words;
			u32 height;
			bool ended = false;
		};

		struct text {
			std::vector<paragraph> paragraphs;
			u32 height;
		};

		std::vector<GUIText::markup_tag> m_tag_stack;

		bool update_style();
		void draw_line(bool lastline);
		void end_fragment();
		void end_word();
		void end_paragraph();
		void push_char(wchar_t c);
		u32 parse_tag(u32 cursor);
		void parse();

		void size(GUIText::text &text);
		void place(GUIText::text &text, core::rect<s32> & text_rect, core::position2d<s32> & offset);
		void draw(GUIText::text &text, core::rect<s32> & text_rect);

		void createVScrollBar();

		core::rect<s32> m_display_text_rect;

		core::position2d<s32> m_text_scrollpos;

		u32 m_scrollbar_width;
		GUIScrollBar *m_vscrollbar;

		style_def m_style;

		fragment m_current_fragment;
		word m_current_word;
		paragraph m_current_paragraph;
		text m_parsed_text;

};


#endif // GUITEXT_HEADER
