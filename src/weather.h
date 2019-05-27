/*
Minetest
Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

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

#pragma once

#include "irrlichttypes.h"
#include <string>

namespace Weather
{
enum Type : u8
{
	NORMAL,
	RAIN,
//	STORM,
	SNOW,
};

struct State
{
	Type type = NORMAL;
	std::string texture;
	float intensity = 1.0f;
	float wind_speed = 0.0f;
	u16 wind_direction = 0;

	void setType(const std::string &strType);
	const std::string &getTypeStr() const;
	std::string getTextureFilename() const;
};
}; // namespace Weather
