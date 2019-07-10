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

class ParsedText
{
	public:
		ParsedText(const wchar_t* text);
		~ParsedText();

		enum ElementType { ELEMENT_TEXT, ELEMENT_SEPARATOR, ELEMENT_IMAGE, ELEMENT_ITEM };
		enum BackgroundType { BACKGROUND_NONE, BACKGROUND_COLOR };
		enum FloatType { FLOAT_NONE, FLOAT_RIGHT, FLOAT_LEFT };
		enum HalignType { HALIGN_CENTER, HALIGN_LEFT, HALIGN_RIGHT, HALIGN_JUSTIFY };
		enum ValignType { VALIGN_MIDDLE, VALIGN_TOP, VALIGN_BOTTOM };

		typedef std::unordered_map<std::string, std::string> StyleList;
		typedef std::unordered_map<std::string, std::string> AttrsList;

		struct Tag {
			std::string name;
			AttrsList attrs;
			StyleList style;
		};

		struct Element {
			std::list<Tag*> tags;
			ElementType type;
			core::stringw text;

			core::dimension2d<u32> dim;
			core::position2d<s32> pos;
			FloatType floating = FLOAT_NONE;

			ValignType valign;
			gui::IGUIFont* font;
			irr::video::SColor color;
			irr::video::SColor hovercolor;

			// img & item specific attributes
			std::string name;
			ItemRotationKind rotation = IT_ROT_NONE;
			s32 margin = 10;

			void setStyle(StyleList &style);
		};

		struct Paragraph {
			std::vector<Element> elements;
			HalignType halign;
			s32 margin = 10;

			void setStyle(StyleList &style);
		};

		std::vector<Paragraph> m_paragraphs;

		// Element style
		s32 margin = 3;
		ValignType valign = VALIGN_TOP;
		BackgroundType background_type = BACKGROUND_NONE;
		irr::video::SColor background_color;

		Tag m_root_tag;

	protected:

		// Parser functions
		void globalTag(ParsedText::AttrsList& attrs);
		void enterElement(ElementType type);
		void endElement();
		void enterParagraph();
		void endParagraph();
		void pushChar(wchar_t c);
		ParsedText::Tag* newTag(std::string name, AttrsList attrs);
		ParsedText::Tag* openTag(std::string, AttrsList attrs);
		bool closeTag(std::string name);
		u32 parseTag(const wchar_t* text, u32 cursor);
		void parse(const wchar_t* text);

		std::vector<Tag *> m_tags;
		std::list<Tag *> m_active_tags;

		// Current values
		StyleList m_style;
		Element* m_element;
		Paragraph* m_paragraph;
};


class TextDrawer
{
	public:
		TextDrawer(
			const wchar_t* text,
			Client *client,
			gui::IGUIEnvironment* environment,
			ISimpleTextureSource *tsrc);
		void place(s32 width);
		inline s32 getHeight() { return m_height; };
		s32 getVoffset(s32 height);
		void draw(core::rect<s32> dest_rect, core::position2d<s32> dest_offset);
		ParsedText::Element* getElementAt(core::position2d<s32> pos);
		ParsedText::Tag* m_hovertag;

	protected:

		struct RectWithMargin {
			core::rect<s32> rect;
			s32 margin;
		};

		ParsedText m_text;
		Client *m_client;
		gui::IGUIEnvironment* m_environment;
		s32 m_height;
		std::vector<RectWithMargin> m_floating;

};

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

		// GUI members
		Client *m_client;
		GUIScrollBar *m_vscrollbar;
		TextDrawer m_drawer;

		// Positionning
		u32 m_scrollbar_width;
		core::rect<s32> m_display_text_rect;
		core::position2d<s32> m_text_scrollpos;

		ParsedText::Element *getElementAt(s32 X, s32 Y);
		void checkHover(s32 X, s32 Y);
};

#endif // GUITEXT_HEADER
