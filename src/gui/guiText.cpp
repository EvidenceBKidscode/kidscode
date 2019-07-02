
#include "IGUIEnvironment.h"
#include "guiScrollBar.h"
#include "IGUIFont.h"
#include <vector>
#include <unordered_map>
#include <cstdlib>
using namespace irr::gui;
#include "guiText.h"
#include "fontengine.h"
#include <SColor.h>
#include "client/tile.h"
#include "IVideoDriver.h"
#include "client.h"

//! constructor
GUIText::GUIText(
	const wchar_t* text,
	IGUIEnvironment* environment,
	IGUIElement* parent, s32 id,
	const core::rect<s32>& rectangle,
	Client *client,
	ISimpleTextureSource *tsrc) :
	IGUIElement(EGUIET_ELEMENT, environment, parent, id, rectangle),
	m_tsrc(tsrc),
	m_client(client),
	m_vscrollbar(NULL),
	m_text_scrollpos(0, 0)
{
	#ifdef _DEBUG
		setDebugName("GUIText");
	#endif

	Text = text;

	createVScrollBar();
}

//! destructor
GUIText::~GUIText()
{
	m_vscrollbar->remove();
}

//! create a vertical scroll bar
void GUIText::createVScrollBar()
{
	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();

	m_scrollbar_width = skin ? skin->getSize(gui::EGDS_SCROLLBAR_SIZE) : 16;

	m_vscrollbar = new GUIScrollBar(Environment, false, this, -1,
		irr::core::rect<s32>(
			RelativeRect.getWidth() - m_scrollbar_width,
			0,
			RelativeRect.getWidth(),
			RelativeRect.getHeight()
		)
	);

	m_vscrollbar->setVisible(false);
	m_vscrollbar->setSmallStep(1);
	m_vscrollbar->setLargeStep(100);
	m_vscrollbar->setPos(0);
	m_vscrollbar->setMax(1);
}

bool GUIText::OnEvent(const SEvent& event) {
	if (event.EventType == EET_GUI_EVENT)
		if (event.GUIEvent.EventType == EGET_SCROLL_BAR_CHANGED)
			if (event.GUIEvent.Caller == m_vscrollbar)
				m_text_scrollpos.Y = -m_vscrollbar->getPos();

	if (event.EventType == EET_MOUSE_INPUT_EVENT)
		if (event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
			m_vscrollbar->setPos(m_vscrollbar->getPos() -
					event.MouseInput.Wheel * m_vscrollbar->getLargeStep());
			m_text_scrollpos.Y = -m_vscrollbar->getPos();
		}

	return IGUIElement::OnEvent(event);
}

//! draws the element and its children
void GUIText::draw()
{
	if (!IsVisible)
		return;

	// Frame
	IGUISkin* skin = Environment->getSkin();
	if (skin)
		skin->draw3DSunkenPane(this, video::SColor(0), false, false,
				AbsoluteRect, &AbsoluteClippingRect);

	// Text
	m_display_text_rect = AbsoluteRect;
	m_display_text_rect.UpperLeftCorner.X ++;
	m_display_text_rect.UpperLeftCorner.Y ++;
	m_display_text_rect.LowerRightCorner.X --;
	m_display_text_rect.LowerRightCorner.Y --;
	parse();
	size(m_parsed_text);
	place(m_parsed_text, m_display_text_rect);

	if (m_parsed_text.height > m_display_text_rect.getHeight()) {

		m_vscrollbar->setMax(m_parsed_text.height - m_display_text_rect.getHeight());
		m_vscrollbar->setVisible(true);

		core::rect<s32> smaller_rect = m_display_text_rect;
		smaller_rect.LowerRightCorner.X -= m_scrollbar_width;
		place(m_parsed_text, smaller_rect);

	} else {
		// TODO: vertical adjust
		m_vscrollbar->setVisible(false);
	}
	draw(m_parsed_text, m_display_text_rect, m_text_scrollpos);

	// draw children
	IGUIElement::draw();
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------

void GUIText::draw(
	GUIText::text & text,
	core::rect<s32> & text_rect,
	core::position2d<s32> & offset)
{
	for (auto & paragraph : text.paragraphs) {
		for (auto & word : paragraph.words)
		{
			if (word.type == t_word) {
				for (auto & fragment : word.fragments)
				{
					core::rect<s32> rect(fragment.position + offset, fragment.dimension);

					if (rect.isRectCollided(text_rect))
						fragment.style.font->draw(fragment.text, rect,
							fragment.style.color, false, true, &text_rect);
				}

			} else if (word.type == t_image) {
				video::ITexture *texture = m_tsrc->getTexture(word.name);
				if (texture != 0)
				{
					core::rect<s32> rect(word.position + offset, word.dimension);
					if (rect.isRectCollided(text_rect))
					{
						Environment->getVideoDriver()->draw2DImage(texture,
							rect, // Dest rect
							irr::core::rect<s32>(core::position2d<s32>(0, 0), texture->getOriginalSize()), // Source rect
							&text_rect, // Clip rect
							0, true); // Colors, use alpha
					}
				} else printf("Texture %s not found!\n", word.name.c_str());

			} else if (word.type == t_item) {
				core::rect<s32> rect(word.position + offset, word.dimension);
				if (rect.isRectCollided(text_rect))
				{
					IItemDefManager *idef = m_client->idef();
					ItemStack item;
					item.deSerialize(word.name, idef);
					drawItemStack(
							Environment->getVideoDriver(), g_fontengine->getFont(), item,
							irr::core::rect<s32>(word.position + offset, word.dimension), &text_rect,
							m_client, IT_ROT_NONE);
							// There is a bug in drawItemStack: no clip when drawing 3D block
					// TODO: Add IT_ROT_SELECTED possibility
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------
// Placing
// -----------------------------------------------------------------------------

void GUIText::size(GUIText::text &text)
{
	for (auto & paragraph : text.paragraphs) {
		for (auto & word : paragraph.words) {
			if (word.type == t_separator || word.type == t_word) {
				word.dimension.Height = 0;
				word.dimension.Width = 0;
				for (auto & fragment : word.fragments) {
					fragment.dimension =
						fragment.style.font->getDimension(fragment.text.c_str());
					if (word.dimension.Height < fragment.dimension.Height)
						word.dimension.Height = fragment.dimension.Height;
					word.dimension.Width += fragment.dimension.Width;
				}
			} else if (word.type == t_image || word.type == t_item) {
				// Dont resize already sized items (sized by another mechanism)
				if (word.dimension.Height == 0 || word.dimension.Width == 0) {
					core::dimension2d<u32> dim(80, 80); // Default image and item size

					if (word.type == t_image) {
						video::ITexture *texture = m_tsrc->getTexture(word.name);
						if (texture != 0)
							dim = texture->getOriginalSize();
					}

					if (word.dimension.Height == 0)
						if (word.dimension.Width == 0)
							word.dimension = dim;
						else
							word.dimension.Height = dim.Height * word.dimension.Width / dim.Width;
					else
						word.dimension.Width = dim.Width * word.dimension.Height / dim.Height;
				}
			}
		}
	}
}

void GUIText::place(
	GUIText::text & text,
	core::rect<s32> & text_rect)
{
	m_floating.clear();
	s32 y = text_rect.UpperLeftCorner.Y;

	for (auto & paragraph : text.paragraphs) {
		paragraph.height = 0;

		// Find and place floating stuff in paragraph
		for (auto word = paragraph.words.begin(); word != paragraph.words.end();
				++word) {
			if (word->floating != floating_none) {
				word->position.Y = y;
				if (word->floating == floating_left)
					word->position.X = text_rect.UpperLeftCorner.X;
				if (word->floating == floating_right)
					word->position.X = text_rect.LowerRightCorner.X -
							word->dimension.Width;

				m_floating.push_back(core::rect<s32>(word->position, word->dimension));
			}
		}

		// Place non floating stuff
		std::vector<GUIText::word>::iterator current_word = paragraph.words.begin();
		while (current_word != paragraph.words.end())
	 	{
			// Determine line width and y pos
			s32 xmin, xmax;
			s32 nexty = y;
			do {
				y = nexty;
				nexty = 0;

				xmin = text_rect.UpperLeftCorner.X;
				xmax = text_rect.LowerRightCorner.X;
				for (auto rect : m_floating) {
					if (rect.UpperLeftCorner.Y <= y && rect.LowerRightCorner.Y >= y) {
						if (!nexty || rect.LowerRightCorner.Y < nexty)
							nexty = rect.LowerRightCorner.Y + 1;
						if (rect.UpperLeftCorner.X <= xmin && rect.LowerRightCorner.X < xmax)
						{ // float on left
							if (rect.LowerRightCorner.X > xmin)
								xmin = rect.LowerRightCorner.X;
						} else if (rect.LowerRightCorner.X >= xmax && rect.UpperLeftCorner.X > xmin)
						{ // float on right
							if (rect.UpperLeftCorner.X < xmax)
								xmax = rect.UpperLeftCorner.X;
						} else if (rect.UpperLeftCorner.X <= xmin && rect.LowerRightCorner.X >= xmax)
						{ // float taking all space
							xmin = xmax;
						} else { // float in the middle -- should not occure yet, see that later
						}
					}
				}
			}
			while (nexty && xmax <= xmin);

			u32 linewidth = xmax - xmin;
			float x = xmin;

			while(current_word != paragraph.words.end() &&
					current_word->type == t_separator) {
				current_word++;
			}

			s32 textwidth = 0;
			s32 textheight = 0;
			u32 wordcount = 0;

			std::vector<GUIText::word>::iterator linestart = current_word;
			std::vector<GUIText::word>::iterator lineend = paragraph.words.end();

			// First pass, find words fitting into line (or at least one world)
			while(current_word != paragraph.words.end() &&
					(textwidth == 0 || textwidth + current_word->dimension.Width <= linewidth))
			{
				if (current_word->floating == floating_none)
				{
					if (current_word->type != t_separator)
					{
						lineend = current_word;
						wordcount++;
					}
					textwidth += current_word->dimension.Width;
				}
				current_word++;
			}

			// Empty line, nothing to place or draw
			if (lineend == paragraph.words.end()) continue;

			lineend++; // Point to the first position outside line (may be end())

			// Second pass, compute printable line width and adjustments
			textwidth = 0;
			for(auto word = linestart; word != lineend; ++word) {
				if (word->floating == floating_none) {
					textwidth += word->dimension.Width;
					if (textheight < word->dimension.Height)
						textheight = word->dimension.Height;
				}
			}

			float extraspace = 0;

			switch(paragraph.style.halign)
			{
				case h_center:
					x += (linewidth - textwidth) / 2;
					break;
				case h_justify:
					if (wordcount > 1 && // Justification only if at least two words.
						!(lineend == paragraph.words.end())) // Don't justify last line.
						extraspace = ((float)(linewidth - textwidth)) / (wordcount - 1);
					break;
				case h_right:
					x += linewidth - textwidth;
			}

			// Third pass, actually place everything
			for(auto word = linestart; word != lineend; ++word) {
				if (word->floating == floating_none) {
					word->position.X = x;
					word->position.Y = y;

					if (word->type == t_word || word->type == t_separator) {
						for (auto fragment = word->fragments.begin();
							fragment != word->fragments.end(); ++fragment)
						{
							fragment->position.X = x;

							switch(fragment->style.valign)
							{
								case v_top:
									fragment->position.Y = y;
									break;
								case v_middle:
									fragment->position.Y = y + (textheight - fragment->dimension.Height) / 2 ;
									break;
								default:
									fragment->position.Y = y + textheight - fragment->dimension.Height ;
							}
							x += fragment->dimension.Width;
						}
						if (word->type == t_separator)
							x += extraspace;
					} else if (word->type == t_image || word->type == t_item) {
						x += word->dimension.Width;
					}
				}
			}
			paragraph.height += textheight;
			y += textheight;
		}
		// TODO: Paragraph spacing to be set in stye
		// TODO: margin managment (overlapping)
		y += 10;
	}
	text.height = y - text_rect.UpperLeftCorner.Y;
	// TODO:check if floating goes under
}

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------


// Code snippet from Galik at https://stackoverflow.com/questions/38812780/split-string-into-key-value-pairs-using-c

std::map<std::string, std::string> get_attributes(std::string const& s)
{
	std::map<std::string, std::string> m;

	std::string::size_type key_pos = 0;
	std::string::size_type key_end;
	std::string::size_type val_pos;
	std::string::size_type val_end;

	while((key_end = s.find('=', key_pos)) != std::string::npos)
	{
		if((val_pos = s.find_first_not_of("= ", key_end)) == std::string::npos)
			break;

		val_end = s.find(' ', val_pos);
		m.emplace(s.substr(key_pos, key_end - key_pos), s.substr(val_pos, val_end - val_pos));

		key_pos = val_end;
		if(key_pos != std::string::npos)
			++key_pos;
	}

	return m;
}

char getwcharhexdigit(char c)
{
	if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) return (char)c;
	if (c >= 'a' && c <= 'f') return (c - 32);
	return 0;
}

bool GUIText::update_style()
{
	GUIText::properties style_properties;
	for (auto const& tag : m_tag_stack)
		for (auto const& prop : tag.style_properties)
			style_properties[prop.first] = prop.second;

	if (style_properties["halign"] == "center")
		m_paragraph_style.halign = h_center;
	else if (style_properties["halign"] == "right")
		m_paragraph_style.halign = h_right;
	else if (style_properties["halign"] == "justify")
		m_paragraph_style.halign = h_justify;
	else
		m_paragraph_style.halign = h_left;

	if (style_properties["valign"] == "middle")
		m_fragment_style.valign = v_middle;
	else if (style_properties["valign"] == "top")
		m_fragment_style.valign = v_top;
	else
		m_fragment_style.valign = v_bottom;

	int r, g, b;
	sscanf(style_properties["color"].c_str(),"%2x%2x%2x", &r, &g, &b);
	m_fragment_style.color = irr::video::SColor(255, r, g, b);

	unsigned int font_size = std::atoi(style_properties["fontsize"].c_str());
	FontMode font_mode = FM_Standard;
	if (style_properties["fontstyle"] == "mono")
		font_mode = FM_Mono;
	m_fragment_style.font = g_fontengine->getFont(font_size, font_mode);

	if (!m_fragment_style.font) {
		printf("No font found ! Size=%d, mode=%d\n", font_size, font_mode);
		return false;
	}

	return true;
}

void GUIText::end_fragment()
{
	if (m_current_fragment.ended) return;
	m_current_fragment.ended = true;
	m_current_word.fragments.push_back(m_current_fragment);
}

void GUIText::end_word()
{
	if (m_current_word.ended) return;

	end_fragment();
	m_current_word.ended = true;
	m_current_paragraph.words.push_back(m_current_word);
}

void GUIText::end_paragraph()
{
	if (m_current_paragraph.ended) return;

	end_word();

	m_current_paragraph.ended = true;

	m_parsed_text.paragraphs.push_back(m_current_paragraph);
}

// Ensure we are in a paragraph
void GUIText::in_paragraph()
{
	if (m_current_paragraph.ended) {
		m_current_paragraph = {};
		m_current_paragraph.style = m_paragraph_style;
	}
}

void GUIText::in_word(wordtype type)
{
	in_paragraph();

	if (m_current_word.type != type)
		end_word();

	if (m_current_word.ended) {
		m_current_word = {};
		m_current_word.type = type;
	}
}

void GUIText::push_char(wchar_t c)
{
	// New word if needed
	if (c == L' ' || c == L'\t')
		in_word(t_separator);
	else
		in_word(t_word);

	// New fragement if needed
	if (m_current_fragment.ended) {
		m_current_fragment = {};
		m_current_fragment.style = m_fragment_style;
	}

	m_current_fragment.text += c;
}

u32 GUIText::parse_tag(u32 cursor)
{
	u32 textsize = Text.size();
	bool tag_end = false;
	bool tag_start = false;
	std::string tag_name = "";
	std::string tag_param = "";
	wchar_t c;

	if (cursor >= textsize) return 0;
	c = Text[cursor];

	if (c == L'/') {
		tag_end = true;
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	while (c != ' ' && c != '>')
	{
		tag_name += (char)c;
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	while (c == ' ') {
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	while (c != '>') {
		tag_param += (char)c;
		if (++cursor >= textsize) return 0;
		c = Text[cursor];
	}

	++cursor; // last '>'

	std::unordered_map<std::string, std::string> properties;

	if (tag_name == "img" || tag_name == "item") {
		if (tag_end) return 0;

		if (tag_name == "img")
			in_word(t_image);
		else
			in_word(t_item);

		std::map<std::string, std::string> attrs = get_attributes(tag_param);

		// Required attributes
		if (!attrs.count("name"))
			return 0;
		m_current_word.name = attrs["name"];

		if (attrs.count("float")) {
			if (attrs["float"] == "left") m_current_word.floating = floating_left;
			if (attrs["float"] == "right") m_current_word.floating = floating_right;
		}

		if (attrs.count("width")) {
			int width = strtol(attrs["width"].c_str(), NULL, 10);
			if (width)
				m_current_word.dimension.Width = width;
		}

		if (attrs.count("height")) {
			int height = strtol(attrs["height"].c_str(), NULL, 10);
			if (height)
				m_current_word.dimension.Height = height;
		}

		end_word();

	} else if (tag_name == "center") {
		if (!tag_end) {
			properties["halign"] = "center";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag_name == "justify") {
		if (!tag_end) {
			properties["halign"] = "justify";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag_name == "left") {
		if (!tag_end) {
			properties["halign"] = "left";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag_name == "right") {
		if (!tag_end) {
			properties["halign"] = "right";
			tag_start = true;
		}
		end_paragraph();
	} else if (tag_name == "small") {
		if (!tag_end) {
			properties["fontsize"] = "16";
			tag_start = true;
		}
		end_fragment();
	} else if (tag_name == "medium") {
		if (!tag_end) {
			properties["fontsize"] = "24";
			tag_start = true;
		}
		end_fragment();
	} else if (tag_name == "large") {
		if (!tag_end) {
			properties["fontsize"] = "36";
			tag_start = true;
		}
		end_fragment();
	} else if (tag_name == "mono") {
		if (!tag_end) {
			properties["fontstyle"] = "mono";
			tag_start = true;
		}
		end_fragment();
	} else if (tag_name == "normal") {
		if (!tag_end) {
			properties["fontstyle"] = "normal";
			tag_start = true;
		}
		end_fragment();
	} else if (tag_name == "color") {
		if (!tag_end) {
			if (tag_param[0] == '#') {
				std::string color = "";
				char c;
				if (tag_param.size() == 4) {
					if (!(c = getwcharhexdigit(tag_param[1]))) return 0;
					color += c; color += c;
					if (!(c = getwcharhexdigit(tag_param[2]))) return 0;
					color += c; color += c;
					if (!(c = getwcharhexdigit(tag_param[3]))) return 0;
					color += c; color += c;
				} else if (tag_param.size() == 7) {
					if (!(c = getwcharhexdigit(tag_param[1]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[2]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[3]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[4]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[5]))) return 0;
					color += c;
					if (!(c = getwcharhexdigit(tag_param[6]))) return 0;
					color += c;
				} else return 0;
				properties["color"] = color;
				tag_start = true;
			} else return 0;
		end_fragment();
		}
	} else return 0; // Unknown tag

	if (tag_start) {
		markup_tag tag;
		tag.name = tag_name;
		tag.style_properties = properties;

		m_tag_stack.push_back(tag);
		update_style();
	}

	// Tag end, unstack last stacked tag with same name
	if (tag_end) {
		bool found = false;
		for (auto tag = m_tag_stack.rbegin(); tag != m_tag_stack.rend(); ++tag)
			if (tag->name == tag_name) {
				m_tag_stack.erase(std::next(tag).base());
				found = true;
				break;
			}
		if (!found) return 0;
		update_style();
	}

	return cursor;
}

void GUIText::parse()
{
	m_parsed_text.paragraphs.clear();

	m_tag_stack.clear();
	markup_tag root_tag;
	root_tag.name = "root";
	root_tag.style_properties["fontsize"] = "12";
	root_tag.style_properties["fontstyle"] = "normal";
	root_tag.style_properties["halign"] = "left";
	root_tag.style_properties["color"] = "FFFFFF";
	m_tag_stack.push_back(root_tag);
	update_style();

	m_current_fragment.ended = true;
	m_current_word.ended = true;
	m_current_paragraph.ended = true;

	u32 cursor = 0;
	bool escape = false;
	wchar_t c;
	u32 textsize = Text.size();
	while (cursor < textsize)
	{
		c = Text[cursor];
		cursor++;

		if (c == L'\r') { // Mac or Windows breaks
			if (cursor < textsize && Text[cursor] == L'\n')
				cursor++;
			end_paragraph();
			escape = false;
			continue;
		}

		if (c == L'\n') { // Unix breaks
			end_paragraph();
			escape = false;
			continue;
		}

		if (escape) {
			escape = false;
			push_char(c);
			continue;
		}

		if (c == L'\\') {
			escape = true;
			continue;
		}
		// Tag check
		if (c == L'<') {
			u32 newcursor = parse_tag(cursor);
			if (newcursor > 0) {
				cursor = newcursor;
				continue;
			}
		}

		// Default behavior
		push_char(c);
	}
	end_paragraph();
}
