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
		typedef std::unordered_map<std::string, std::string> KeyValues;

		struct markup_tag;

		struct fragment {
			core::dimension2d<u32> dimension;
			core::position2d<s32> position;
			core::stringw text;
			valign_type valign;
			gui::IGUIFont* font;
			irr::video::SColor color;
			std::list<s32> tag_ids;
			void set_style(KeyValues &style);
		};

		struct word {
			wordtype type;
			core::dimension2d<u32> dimension;
			core::position2d<s32> position;
			std::vector<fragment> fragments;
			std::string name;
			floating_type floating = floating_none;
		};

		struct paragraph {
			std::vector<word> words;
			halign_type halign;
			u32 height;
			void set_style(KeyValues &style);
		};

		struct text {
			std::vector<paragraph> paragraphs;
			u32 height;
		};

		struct markup_tag {
			std::string name;
			KeyValues attrs;
			KeyValues style;
			std::vector<fragment *> fragments;
			std::string link;
		};

		void size(GUIText::text &text);
		void place(GUIText::text &text, core::rect<s32> & text_rect);
		void draw(GUIText::text &text, core::rect<s32> & text_rect);
		GUIText::word* getWordAt(s32 X, s32 Y);
		GUIText::fragment* getFragmentAt(s32 X, s32 Y);
		void checkHover(s32 X, s32 Y);

		// Parser functions
		void update_style();
		void enter_fragment(wordtype type);
		void end_fragment();
		void enter_word(wordtype type);
		void end_word();
		void enter_paragraph();
		void end_paragraph();
		void push_char(wchar_t c);
		u32 parse_tag(u32 cursor);
		KeyValues get_attributes(std::string const& s);
		void parse();
		markup_tag* get_tag_for_fragment(std::string tag_name);

		// GUI members
		ISimpleTextureSource *m_tsrc;
		Client *m_client;
		GUIScrollBar *m_vscrollbar;

		// Positionning
		u32 m_scrollbar_width;
		core::rect<s32> m_display_text_rect;
		core::position2d<s32> m_text_scrollpos;
		s32 m_hover_tag_id = 0;

		// Raw unparsed text in a single string
		core::stringw m_raw_text;

		// Parsed text broke down into paragraphs / words / fragments + layout info
		text m_parsed_text;

		std::vector<core::rect<s32>> m_floating;
		std::vector<markup_tag> m_tags;

		// Parser vars
		std::list<s32> m_active_tag_ids;
		KeyValues m_current_style;

		fragment * m_current_fragment = 0;
		word * m_current_word = 0;
		paragraph * m_current_paragraph = 0;
};


#endif // GUITEXT_HEADER
