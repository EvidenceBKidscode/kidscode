/*
Copyright (C) 2013 xyz, Ilya Zhuravlev <whatever@xyz.is>
Copyright (C) 2016 Nore, Nathanaël Courant <nore@mesecons.net>

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

#include "enriched_string.h"
#include "util/string.h"
#include "log.h"
using namespace irr::video;

EnrichedString::EnrichedString()
{
	clear();
}

EnrichedString::EnrichedString(const std::wstring &string,
		const std::vector<SColor> &colors):
	m_string(string),
	m_colors(colors)
{}

EnrichedString::EnrichedString(const std::wstring &s, const SColor &color)
{
	clear();
	addAtEnd(translate_string(s), color);
}

EnrichedString::EnrichedString(const wchar_t *str, const SColor &color)
{
	clear();
	addAtEnd(translate_string(std::wstring(str)), color);
}

void EnrichedString::operator=(const wchar_t *str)
{
	clear();
	addAtEnd(translate_string(std::wstring(str)), SColor(255, 255, 255, 255));
}

void EnrichedString::addAtEnd(const std::wstring &s, const SColor &initial_color)
{
	SColor color(initial_color);
	size_t i = 0;
	while (i < s.length()) {
		if (s[i] != L'\x1b') {
			m_string += s[i];
			m_colors.push_back(color);
			++i;
			continue;
		}
		++i;
		size_t start_index = i;
		size_t length;
		if (i == s.length()) {
			break;
		}
		if (s[i] == L'(') {
			++i;
			++start_index;
			while (i < s.length() && s[i] != L')') {
				if (s[i] == L'\\') {
					++i;
				}
				++i;
			}
			length = i - start_index;
			++i;
		} else {
			++i;
			length = 1;
		}
		std::wstring escape_sequence(s, start_index, length);
		std::vector<std::wstring> parts = split(escape_sequence, L'@');
		if (parts[0] == L"c") {
			if (parts.size() < 2) {
				continue;
			}
			parseColorString(wide_to_utf8(parts[1]), color, true);
		} else if (parts[0] == L"b") {
			if (parts.size() < 2) {
				continue;
			}
			parseColorString(wide_to_utf8(parts[1]), m_background, true);
			m_has_background = true;
		}
	}
}

void EnrichedString::addChar(const EnrichedString &source, size_t i)
{
	m_string += source.m_string[i];
	m_colors.push_back(source.m_colors[i]);
}

void EnrichedString::addCharNoColor(wchar_t c)
{
	m_string += c;
	if (m_colors.empty()) {
		m_colors.emplace_back(255, 255, 255, 255);
	} else {
		m_colors.push_back(m_colors[m_colors.size() - 1]);
	}
}

EnrichedString EnrichedString::operator+(const EnrichedString &other) const
{
	std::vector<SColor> result;
	result.insert(result.end(), m_colors.begin(), m_colors.end());
	result.insert(result.end(), other.m_colors.begin(), other.m_colors.end());
	return EnrichedString(m_string + other.m_string, result);
}

void EnrichedString::operator+=(const EnrichedString &other)
{
	m_string += other.m_string;
	m_colors.insert(m_colors.end(), other.m_colors.begin(), other.m_colors.end());
}

EnrichedString EnrichedString::substr(size_t pos, size_t len) const
{
	if (pos == m_string.length()) {
		return EnrichedString();
	}
	if (len == std::string::npos || pos + len > m_string.length()) {
		return EnrichedString(
			m_string.substr(pos, std::string::npos),
			std::vector<SColor>(m_colors.begin() + pos, m_colors.end())
		);
	}

	return EnrichedString(
		m_string.substr(pos, len),
		std::vector<SColor>(m_colors.begin() + pos, m_colors.begin() + pos + len)
	);

}

const wchar_t *EnrichedString::c_str() const
{
	return m_string.c_str();
}

const std::vector<SColor> &EnrichedString::getColors() const
{
	return m_colors;
}

const std::wstring &EnrichedString::getString() const
{
	return m_string;
}


const std::wstring EnrichedString::getFixedString() const // :PATCH:
{
	std::wstring fixed_string;
	
	for (int i = 0; i < m_string.size(); ++i)
	{
		unsigned c = m_string.at(i);
		if (c >= 0xc3 && i+1 < m_string.size()) {
			unsigned c2 = m_string.at(i+1);
			if (c2 >= 0xA0 && c2 <= 0xBF) {
				// lowercase letters
				c = c2 + 0x40;
			} else {
				// uppercase letters
				switch (c2) {
					case 0x20AC : c = 0xC0; break; // À
					case 0x81 : c = 0xC1; break; // Á
					case 0x201A : c = 0xC2; break; // Â
					case 0x192 : c = 0xC3; break; // Ã
					case 0x201E : c = 0xC4; break; // Ä
					case 0x2026 : c = 0xC5; break; // Å
					case 0x2020 : c = 0xC6; break; // Æ
					case 0x2021 : c = 0xC7; break; // Ç
					case 0x2C6 : c = 0xC8; break; // È
					case 0x2030 : c = 0xC9; break; // É
					case 0x160 : c = 0xCA; break; // Ê
					case 0x2039 : c = 0xCB; break; // Ë
					case 0x152 : c = 0xCC; break; // Ì
					case 0x8D : c = 0xCD; break; // Í
					case 0x17D : c = 0xCE; break; // Î
					case 0x8F : c = 0xCF; break; // Ï
					case 0x2018 : c = 0xD1; break; // Ñ
					case 0x2019 : c = 0xD2; break; // Ò
					case 0x201C : c = 0xD3; break; // Ó
					case 0x201D : c = 0xD4; break; // Ô
					case 0x2022 : c = 0xD5; break; // Õ
					case 0x2013 : c = 0xD6; break; // Ö
					case 0x2122 : c = 0xD9; break; // Ù
					case 0x161 : c = 0xDA; break; // Ú
					case 0x203A : c = 0xDB; break; // Û
					case 0x153 : c = 0xDC; break; // Ü
					case 0x9D : c = 0xDD; break; // Ý
					default : c='?';
				}
			}
			++i;
		}
		fixed_string += c;
	}
	return fixed_string;
}
