#ifndef GUITEXT_HEADER
#define GUITEXT_HEADER

#include "IGUIElement.h"

using namespace irr;

class ISimpleTextureSource;
class Client;

class GUIText : public gui::IGUIElement
{
	public:

		//! constructor
		GUIText(
			const wchar_t* text,
			gui::IGUIEnvironment* environment,
			gui::IGUIElement* parent,
			s32 id,
			const core::rect<s32>& rectangle,
			Client *client,
			ISimpleTextureSource *tsrc);

		//! destructor
		virtual ~GUIText();

		//! draws the element and its children
		virtual void draw();

		bool OnEvent(const SEvent& event);

	protected:

		enum halign_type { h_center, h_left, h_right, h_justify };
		enum valign_type { v_middle, v_top, v_bottom };
		enum wordtype { t_word, t_separator, t_image, t_item };
		enum floating_type { floating_none, floating_right, floating_left };
		typedef std::unordered_map<std::string, std::string> properties;

		struct markup_tag {
			std::string name;
			GUIText::properties style_properties;
		};

		struct fragment_style {
			valign_type valign;
			gui::IGUIFont* font;
			irr::video::SColor color;
		};

		struct paragraph_style {
			halign_type halign;
		};

		struct fragment {
			bool ended = false;
			fragment_style style;
			core::stringw text;
			core::dimension2d<u32> dimension;
			core::position2d<s32> position;
			bool draw = false;
		};

		struct word {
			bool ended = false;
			std::vector<fragment> fragments;
			core::dimension2d<u32> dimension;
			core::position2d<s32> position;
			wordtype type;
			floating_type floating = floating_none;
			std::string name;
			bool draw = false;
		};

		struct paragraph {
			bool ended = false;
			paragraph_style style;
			std::vector<word> words;
			u32 height;
		};
/*
		struct image {
			std::string name;
			core::dimension2d<u32> dimension;
			core::position2d<s32> position;
			bool draw = false;
		};
*/
		struct text {
			std::vector<paragraph> paragraphs;
//			std::vector<image> images;
			u32 height;
		};

		std::vector<markup_tag> m_tag_stack;

		bool update_style();
		void draw_line(bool lastline);
		void end_fragment();
		void in_word(wordtype type);
		void end_word();
		void in_paragraph();
		void end_paragraph();
		void push_char(wchar_t c);
		u32 parse_tag(u32 cursor);
		void parse();

		void size(GUIText::text &text);
		void place(GUIText::text &text, core::rect<s32> & text_rect, core::position2d<s32> & offset);
		void draw(GUIText::text &text, core::rect<s32> & text_rect);

		void createVScrollBar();

		ISimpleTextureSource *m_tsrc;
		Client *m_client;

		core::rect<s32> m_display_text_rect;
		core::position2d<s32> m_text_scrollpos;

		u32 m_scrollbar_width;
		GUIScrollBar *m_vscrollbar;


		paragraph_style m_paragraph_style;
		fragment_style m_fragment_style;

		fragment m_current_fragment;
		word m_current_word;
		paragraph m_current_paragraph;
		text m_parsed_text;

		std::vector<core::rect<s32>> m_floating;

};


#endif // GUITEXT_HEADER
