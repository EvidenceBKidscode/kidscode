/*
Minetest
Copyright (C) 2019 rubenwardy

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

#include "client/tile.h" // ITextureSource
#include "debug.h"
#include "irrlichttypes_extrabloated.h"
#include "util/string.h"
#include <array>

#pragma once

class StyleSpec
{
public:
	enum Property
	{
		TEXTCOLOR,
		BGCOLOR,
		BGCOLOR_HOVERED, // Note: Deprecated property
		BGCOLOR_PRESSED, // Note: Deprecated property
		NOCLIP,
		BORDER,
		BGIMG,
		BGIMG_HOVERED, // Note: Deprecated property
		BGIMG_MIDDLE,
		BGIMG_PRESSED, // Note: Deprecated property
		FGIMG,
		FGIMG_HOVERED, // Note: Deprecated property
		FGIMG_PRESSED, // Note: Deprecated property
		ALPHA,
		CONTENT_OFFSET,
		PADDING,
		NUM_PROPERTIES,
		NONE
	};
	enum State
	{
		STATE_DEFAULT = 0,
		STATE_HOVERED = 1 << 0,
		STATE_PRESSED = 1 << 1,
		NUM_STATES = 1 << 2,
		STATE_INVALID = 1 << 3,
	};

private:
	std::array<bool, NUM_PROPERTIES> property_set{};
	std::array<std::string, NUM_PROPERTIES> properties;
	State state_map = STATE_DEFAULT;

public:
	static Property GetPropertyByName(const std::string &name)
	{
		if (name == "textcolor") {
			return TEXTCOLOR;
		} else if (name == "bgcolor") {
			return BGCOLOR;
		} else if (name == "bgcolor_hovered") {
			return BGCOLOR_HOVERED;
		} else if (name == "bgcolor_pressed") {
			return BGCOLOR_PRESSED;
		} else if (name == "noclip") {
			return NOCLIP;
		} else if (name == "border") {
			return BORDER;
		} else if (name == "bgimg") {
			return BGIMG;
		} else if (name == "bgimg_hovered") {
			return BGIMG_HOVERED;
		} else if (name == "bgimg_middle") {
			return BGIMG_MIDDLE;
		} else if (name == "bgimg_pressed") {
			return BGIMG_PRESSED;
		} else if (name == "fgimg") {
			return FGIMG;
		} else if (name == "fgimg_hovered") {
			return FGIMG_HOVERED;
		} else if (name == "fgimg_pressed") {
			return FGIMG_PRESSED;
		} else if (name == "alpha") {
			return ALPHA;
		} else if (name == "content_offset") {
			return CONTENT_OFFSET;
		} else if (name == "padding") {
			return PADDING;
		} else {
			return NONE;
		}
	}

	std::string get(Property prop, std::string def) const
	{
		const auto &val = properties[prop];
		return val.empty() ? def : val;
	}

	void set(Property prop, const std::string &value)
	{
		properties[prop] = value;
		property_set[prop] = true;
	}

	//! Parses a name and returns the corresponding state enum
	static State getStateByName(const std::string &name)
	{
		if (name == "default") {
			return STATE_DEFAULT;
		} else if (name == "hovered") {
			return STATE_HOVERED;
		} else if (name == "pressed") {
			return STATE_PRESSED;
		} else {
			return STATE_INVALID;
		}
	}

	//! Gets the state that this style is intended for
	State getState() const
	{
		return state_map;
	}

	//! Set the given state on this style
	void addState(State state)
	{
		FATAL_ERROR_IF(state >= NUM_STATES, "Out-of-bounds state received");

		state_map = static_cast<State>(state_map | state);
	}

	//! Using a list of styles mapped to state values, calculate the final
	//  combined style for a state by propagating values in its component states
	static StyleSpec getStyleFromStatePropagation(const std::array<StyleSpec, NUM_STATES> &styles, State state)
	{
		StyleSpec temp = styles[StyleSpec::STATE_DEFAULT];
		temp.state_map = state;
		for (int i = StyleSpec::STATE_DEFAULT + 1; i <= state; i++) {
			if ((state & i) != 0) {
				temp = temp | styles[i];
			}
		}

		return temp;
	}

	video::SColor getColor(Property prop, video::SColor def) const
	{
		const auto &val = properties[prop];
		if (val.empty()) {
			return def;
		}

		parseColorString(val, def, false, 0xFF);
		return def;
	}

	video::SColor getColor(Property prop) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		video::SColor color;
		parseColorString(val, color, false, 0xFF);
		return color;
	}

	irr::core::rect<s32> getRect(Property prop, irr::core::rect<s32> def) const
	{
		const auto &val = properties[prop];
		if (val.empty())
			return def;

		irr::core::rect<s32> rect;
		if (!parseRect(val, &rect))
			return def;

		return rect;
	}

	irr::core::rect<s32> getRect(Property prop) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		irr::core::rect<s32> rect;
		parseRect(val, &rect);
		return rect;
	}

	irr::core::vector2d<s32> getVector2i(Property prop, irr::core::vector2d<s32> def) const
	{
		const auto &val = properties[prop];
		if (val.empty())
			return def;

		irr::core::vector2d<s32> vec;
		if (!parseVector2i(val, &vec))
			return def;

		return vec;
	}

	irr::core::vector2d<s32> getVector2i(Property prop) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		irr::core::vector2d<s32> vec;
		parseVector2i(val, &vec);
		return vec;
	}

	video::ITexture *getTexture(Property prop, ISimpleTextureSource *tsrc,
			video::ITexture *def) const
	{
		const auto &val = properties[prop];
		if (val.empty()) {
			return def;
		}

		video::ITexture *texture = tsrc->getTexture(val);

		return texture;
	}

	video::ITexture *getTexture(Property prop, ISimpleTextureSource *tsrc) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		video::ITexture *texture = tsrc->getTexture(val);

		return texture;
	}

	bool getBool(Property prop, bool def) const
	{
		const auto &val = properties[prop];
		if (val.empty()) {
			return def;
		}

		return is_yes(val);
	}

	inline bool isNotDefault(Property prop) const
	{
		return !properties[prop].empty();
	}

	inline bool hasProperty(Property prop) const { return property_set[prop]; }

	StyleSpec &operator|=(const StyleSpec &other)
	{
		for (size_t i = 0; i < NUM_PROPERTIES; i++) {
			auto prop = (Property)i;
			if (other.hasProperty(prop)) {
				set(prop, other.get(prop, ""));
			}
		}

		return *this;
	}

	StyleSpec operator|(const StyleSpec &other) const
	{
		StyleSpec newspec = *this;
		newspec |= other;
		return newspec;
	}

private:
	bool parseRect(const std::string &value, irr::core::rect<s32> *parsed_rect) const
	{
		irr::core::rect<s32> rect;
		std::vector<std::string> v_rect = split(value, ',');

		if (v_rect.size() == 1) {
			s32 x = stoi(v_rect[0]);
			rect.UpperLeftCorner = irr::core::vector2di(x, x);
			rect.LowerRightCorner = irr::core::vector2di(-x, -x);
		} else if (v_rect.size() == 2) {
			s32 x = stoi(v_rect[0]);
			s32 y =	stoi(v_rect[1]);
			rect.UpperLeftCorner = irr::core::vector2di(x, y);
			rect.LowerRightCorner = irr::core::vector2di(-x, -y);
			// `-x` is interpreted as `w - x`
		} else if (v_rect.size() == 4) {
			rect.UpperLeftCorner = irr::core::vector2di(
					stoi(v_rect[0]), stoi(v_rect[1]));
			rect.LowerRightCorner = irr::core::vector2di(
					stoi(v_rect[2]), stoi(v_rect[3]));
		} else {
			warningstream << "Invalid rectangle string format: \"" << value
					<< "\"" << std::endl;
			return false;
		}

		*parsed_rect = rect;

		return true;
	}

	bool parseVector2i(const std::string &value, irr::core::vector2d<s32> *parsed_vec) const
	{
		irr::core::vector2d<s32> vec;
		std::vector<std::string> v_vector = split(value, ',');

		if (v_vector.size() == 1) {
			s32 x = stoi(v_vector[0]);
			vec.X = x;
			vec.Y = x;
		} else if (v_vector.size() == 2) {
			s32 x = stoi(v_vector[0]);
			s32 y =	stoi(v_vector[1]);
			vec.X = x;
			vec.Y = y;
		} else {
			warningstream << "Invalid vector2d string format: \"" << value
					<< "\"" << std::endl;
			return false;
		}

		*parsed_vec = vec;

		return true;
	}
};
