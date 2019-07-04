/*
Minetest
Copyright (C) 2019 EvicenceBKidscode / Pierre-Yves Rollo <dev@pyrollo.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef GUITEXT_HEADER
#define GUITEXT_HEADER

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

			// word & separator specific attributes
			std::vector<fragment> fragments;

			// img & item specific attributes
			std::string name;
			ItemRotationKind rotation = IT_ROT_NONE;
			floating_type floating = floating_none;
			s32 margin = 10;
		};

		struct paragraph {
			std::vector<word> words;
			halign_type halign;
			u32 height;
			void set_style(KeyValues &style);
			s32 margin = 10;
		};

		struct text {
			std::vector<paragraph> paragraphs;
			u32 height;
			s32 margin = 5;
		};

		struct markup_tag {
			std::string name;
			KeyValues attrs;
			KeyValues style;
			std::vector<fragment *> fragments;
			std::string link;
		};

		struct rect_with_margin {
			core::rect<s32> rect;
			s32 margin;
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

		std::vector<rect_with_margin> m_floating;
		std::vector<markup_tag> m_tags;

		// Parser vars
		std::list<s32> m_active_tag_ids;
		KeyValues m_current_style;

		fragment * m_current_fragment = 0;
		word * m_current_word = 0;
		paragraph * m_current_paragraph = 0;
};


#endif // GUITEXT_HEADER
