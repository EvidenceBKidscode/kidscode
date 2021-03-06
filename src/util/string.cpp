/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "string.h"
#include "pointer.h"
#include "numeric.h"
#include "log.h"

#include "hex.h"
#include "porting.h"
#include "translation.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <map>

#ifndef _WIN32
	#include <iconv.h>
#else
	#define _WIN32_WINNT 0x0501
	#include <windows.h>
#endif

#if defined(_ICONV_H_) && (defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__))
	#define BSD_ICONV_USED
#endif

static bool parseHexColorString(const std::string &value, video::SColor &color,
		unsigned char default_alpha = 0xff);
static bool parseNamedColorString(const std::string &value, video::SColor &color);

#ifndef _WIN32

bool convert(const char *to, const char *from, char *outbuf,
		size_t outbuf_size, char *inbuf, size_t inbuf_size)
{
	iconv_t cd = iconv_open(to, from);

#ifdef BSD_ICONV_USED
	const char *inbuf_ptr = inbuf;
#else
	char *inbuf_ptr = inbuf;
#endif

	char *outbuf_ptr = outbuf;

	size_t *inbuf_left_ptr = &inbuf_size;
	size_t *outbuf_left_ptr = &outbuf_size;

	size_t old_size = inbuf_size;
	while (inbuf_size > 0) {
		iconv(cd, &inbuf_ptr, inbuf_left_ptr, &outbuf_ptr, outbuf_left_ptr);
		if (inbuf_size == old_size) {
			iconv_close(cd);
			return false;
		}
		old_size = inbuf_size;
	}

	iconv_close(cd);
	return true;
}

#ifdef __ANDROID__
// Android need manual caring to support the full character set possible with wchar_t
const char *DEFAULT_ENCODING = "UTF-32LE";
#else
const char *DEFAULT_ENCODING = "WCHAR_T";
#endif

std::wstring utf8_to_wide(const std::string &input)
{
	size_t inbuf_size = input.length() + 1;
	// maximum possible size, every character is sizeof(wchar_t) bytes
	size_t outbuf_size = (input.length() + 1) * sizeof(wchar_t);

	char *inbuf = new char[inbuf_size];
	memcpy(inbuf, input.c_str(), inbuf_size);
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);

#ifdef __ANDROID__
	// Android need manual caring to support the full character set possible with wchar_t
	SANITY_CHECK(sizeof(wchar_t) == 4);
#endif

	if (!convert(DEFAULT_ENCODING, "UTF-8", outbuf, outbuf_size, inbuf, inbuf_size)) {
		infostream << "Couldn't convert UTF-8 string 0x" << hex_encode(input)
			<< " into wstring" << std::endl;
		delete[] inbuf;
		delete[] outbuf;
		return L"<invalid UTF-8 string>";
	}
	std::wstring out((wchar_t *)outbuf);

	delete[] inbuf;
	delete[] outbuf;

	return out;
}

std::string wide_to_utf8(const std::wstring &input)
{
	size_t inbuf_size = (input.length() + 1) * sizeof(wchar_t);
	// maximum possible size: utf-8 encodes codepoints using 1 up to 6 bytes
	size_t outbuf_size = (input.length() + 1) * 6;

	char *inbuf = new char[inbuf_size];
	memcpy(inbuf, input.c_str(), inbuf_size);
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);

	if (!convert("UTF-8", DEFAULT_ENCODING, outbuf, outbuf_size, inbuf, inbuf_size)) {
		infostream << "Couldn't convert wstring 0x" << hex_encode(inbuf, inbuf_size)
			<< " into UTF-8 string" << std::endl;
		delete[] inbuf;
		delete[] outbuf;
		return "<invalid wstring>";
	}
	std::string out(outbuf);

	delete[] inbuf;
	delete[] outbuf;

	return out;
}

#else // _WIN32

std::wstring utf8_to_wide(const std::string &input)
{
	size_t outbuf_size = input.size() + 1;
	wchar_t *outbuf = new wchar_t[outbuf_size];
	memset(outbuf, 0, outbuf_size * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.size(),
		outbuf, outbuf_size);
	std::wstring out(outbuf);
	delete[] outbuf;
	return out;
}

std::string wide_to_utf8(const std::wstring &input)
{
	size_t outbuf_size = (input.size() + 1) * 6;
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);
	WideCharToMultiByte(CP_UTF8, 0, input.c_str(), input.size(),
		outbuf, outbuf_size, NULL, NULL);
	std::string out(outbuf);
	delete[] outbuf;
	return out;
}

#endif // _WIN32

// You must free the returned string!
// The returned string is allocated using new
wchar_t *utf8_to_wide_c(const char *str)
{
	std::wstring ret = utf8_to_wide(std::string(str));
	size_t len = ret.length();
	wchar_t *ret_c = new wchar_t[len + 1];
	memset(ret_c, 0, (len + 1) * sizeof(wchar_t));
	memcpy(ret_c, ret.c_str(), len * sizeof(wchar_t));
	return ret_c;
}

// You must free the returned string!
// The returned string is allocated using new
wchar_t *narrow_to_wide_c(const char *str)
{
	wchar_t *nstr = nullptr;
#if defined(_WIN32)
	int nResult = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) str, -1, 0, 0);
	if (nResult == 0) {
		errorstream<<"gettext: MultiByteToWideChar returned null"<<std::endl;
	} else {
		nstr = new wchar_t[nResult];
		MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) str, -1, (WCHAR *) nstr, nResult);
	}
#else
	size_t len = strlen(str);
	nstr = new wchar_t[len + 1];

	std::wstring intermediate = narrow_to_wide(str);
	memset(nstr, 0, (len + 1) * sizeof(wchar_t));
	memcpy(nstr, intermediate.c_str(), len * sizeof(wchar_t));
#endif

	return nstr;
}

std::wstring narrow_to_wide(const std::string &mbs) {
#ifdef __ANDROID__
	return utf8_to_wide(mbs);
#else
	size_t wcl = mbs.size();
	Buffer<wchar_t> wcs(wcl + 1);
	size_t len = mbstowcs(*wcs, mbs.c_str(), wcl);
	if (len == (size_t)(-1))
		return L"<invalid multibyte string>";
	wcs[len] = 0;
	return *wcs;
#endif
}


std::string wide_to_narrow(const std::wstring &wcs)
{
#ifdef __ANDROID__
	return wide_to_utf8(wcs);
#else
	size_t mbl = wcs.size() * 4;
	SharedBuffer<char> mbs(mbl+1);
	size_t len = wcstombs(*mbs, wcs.c_str(), mbl);
	if (len == (size_t)(-1))
		return "Character conversion failed!";

	mbs[len] = 0;
	return *mbs;
#endif
}


std::string urlencode(const std::string &str)
{
	// Encodes non-unreserved URI characters by a percent sign
	// followed by two hex digits. See RFC 3986, section 2.3.
	static const char url_hex_chars[] = "0123456789ABCDEF";
	std::ostringstream oss(std::ios::binary);
	for (unsigned char c : str) {
		if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
			oss << c;
		} else {
			oss << "%"
				<< url_hex_chars[(c & 0xf0) >> 4]
				<< url_hex_chars[c & 0x0f];
		}
	}
	return oss.str();
}

std::string urldecode(const std::string &str)
{
	// Inverse of urlencode
	std::ostringstream oss(std::ios::binary);
	for (u32 i = 0; i < str.size(); i++) {
		unsigned char highvalue, lowvalue;
		if (str[i] == '%' &&
				hex_digit_decode(str[i+1], highvalue) &&
				hex_digit_decode(str[i+2], lowvalue)) {
			oss << (char) ((highvalue << 4) | lowvalue);
			i += 2;
		} else {
			oss << str[i];
		}
	}
	return oss.str();
}

u32 readFlagString(std::string str, const FlagDesc *flagdesc, u32 *flagmask)
{
	u32 result = 0;
	u32 mask = 0;
	char *s = &str[0];
	char *flagstr;
	char *strpos = nullptr;

	while ((flagstr = strtok_r(s, ",", &strpos))) {
		s = nullptr;

		while (*flagstr == ' ' || *flagstr == '\t')
			flagstr++;

		bool flagset = true;
		if (!strncasecmp(flagstr, "no", 2)) {
			flagset = false;
			flagstr += 2;
		}

		for (int i = 0; flagdesc[i].name; i++) {
			if (!strcasecmp(flagstr, flagdesc[i].name)) {
				mask |= flagdesc[i].flag;
				if (flagset)
					result |= flagdesc[i].flag;
				break;
			}
		}
	}

	if (flagmask)
		*flagmask = mask;

	return result;
}

std::string writeFlagString(u32 flags, const FlagDesc *flagdesc, u32 flagmask)
{
	std::string result;

	for (int i = 0; flagdesc[i].name; i++) {
		if (flagmask & flagdesc[i].flag) {
			if (!(flags & flagdesc[i].flag))
				result += "no";

			result += flagdesc[i].name;
			result += ", ";
		}
	}

	size_t len = result.length();
	if (len >= 2)
		result.erase(len - 2, 2);

	return result;
}

size_t mystrlcpy(char *dst, const char *src, size_t size)
{
	size_t srclen  = strlen(src) + 1;
	size_t copylen = MYMIN(srclen, size);

	if (copylen > 0) {
		memcpy(dst, src, copylen);
		dst[copylen - 1] = '\0';
	}

	return srclen;
}

char *mystrtok_r(char *s, const char *sep, char **lasts)
{
	char *t;

	if (!s)
		s = *lasts;

	while (*s && strchr(sep, *s))
		s++;

	if (!*s)
		return nullptr;

	t = s;
	while (*t) {
		if (strchr(sep, *t)) {
			*t++ = '\0';
			break;
		}
		t++;
	}

	*lasts = t;
	return s;
}

u64 read_seed(const char *str)
{
	char *endptr;
	u64 num;

	if (str[0] == '0' && str[1] == 'x')
		num = strtoull(str, &endptr, 16);
	else
		num = strtoull(str, &endptr, 10);

	if (*endptr)
		num = murmur_hash_64_ua(str, (int)strlen(str), 0x1337);

	return num;
}

void parseTextString(const std::string &value, std::string &text, std::string &params,
		const char sep, const char esc)
{
	u32 i;
	for (i = 0; i < value.length(); ++i) {
		char c = value[i];

		if (c == esc)
			++i;
		else if (c == sep)
			break;
	}

	if (i + 1 < value.length())
		params = value.substr(i + 1);
	else
		params = "";

	text = value.substr(0, i);
}

bool parseColorString(const std::string &value, video::SColor &color, bool quiet,
		unsigned char default_alpha)
{
	bool success;

	if (value[0] == '#')
		success = parseHexColorString(value, color, default_alpha);
	else
		success = parseNamedColorString(value, color);

	if (!success && !quiet)
		errorstream << "Invalid color: \"" << value << "\"" << std::endl;

	return success;
}

static bool parseHexColorString(const std::string &value, video::SColor &color,
		unsigned char default_alpha)
{
	unsigned char components[] = { 0x00, 0x00, 0x00, default_alpha }; // R,G,B,A

	if (value[0] != '#')
		return false;

	size_t len = value.size();
	bool short_form;

	if (len == 9 || len == 7) // #RRGGBBAA or #RRGGBB
		short_form = false;
	else if (len == 5 || len == 4) // #RGBA or #RGB
		short_form = true;
	else
		return false;

	bool success = true;

	for (size_t pos = 1, cc = 0; pos < len; pos++, cc++) {
		assert(cc < sizeof components / sizeof components[0]);
		if (short_form) {
			unsigned char d;
			if (!hex_digit_decode(value[pos], d)) {
				success = false;
				break;
			}
			components[cc] = (d & 0xf) << 4 | (d & 0xf);
		} else {
			unsigned char d1, d2;
			if (!hex_digit_decode(value[pos], d1) ||
					!hex_digit_decode(value[pos+1], d2)) {
				success = false;
				break;
			}
			components[cc] = (d1 & 0xf) << 4 | (d2 & 0xf);
			pos++;	// skip the second digit -- it's already used
		}
	}

	if (success) {
		color.setRed(components[0]);
		color.setGreen(components[1]);
		color.setBlue(components[2]);
		color.setAlpha(components[3]);
	}

	return success;
}

static bool parseNamedColorString(const std::string &value, video::SColor &color)
{
	std::string color_name;
	std::string alpha_string;

	/* If the string has a # in it, assume this is the start of a specified
	 * alpha value (if it isn't the string is invalid and the error will be
	 * caught later on, either because the color name won't be found or the
	 * alpha value will fail conversion)
	 */
	size_t alpha_pos = value.find('#');
	if (alpha_pos != std::string::npos) {
		color_name = value.substr(0, alpha_pos);
		alpha_string = value.substr(alpha_pos + 1);
	} else {
		color_name = value;
	}

	color_name = lowercase(value);

	std::map<const std::string, unsigned>::const_iterator it;
	it = NAMED_COLORS.colors.find(color_name);
	if (it == NAMED_COLORS.colors.end())
		return false;

	u32 color_temp = it->second;

	/* An empty string for alpha is ok (none of the color table entries
	 * have an alpha value either). Color strings without an alpha specified
	 * are interpreted as fully opaque
	 *
	 * For named colors the supplied alpha string (representing a hex value)
	 * must be exactly two digits. For example:  colorname#08
	 */
	if (!alpha_string.empty()) {
		if (alpha_string.length() != 2)
			return false;

		unsigned char d1, d2;
		if (!hex_digit_decode(alpha_string.at(0), d1)
				|| !hex_digit_decode(alpha_string.at(1), d2))
			return false;
		color_temp |= ((d1 & 0xf) << 4 | (d2 & 0xf)) << 24;
	} else {
		color_temp |= 0xff << 24;  // Fully opaque
	}

	color = video::SColor(color_temp);

	return true;
}

void str_replace(std::string &str, char from, char to)
{
	std::replace(str.begin(), str.end(), from, to);
}

/* Translated strings have the following format:
 * \x1bT marks the beginning of a translated string
 * \x1bE marks its end
 *
 * \x1bF marks the beginning of an argument, and \x1bE its end.
 *
 * Arguments are *not* translated, as they may contain escape codes.
 * Thus, if you want a translated argument, it should be inside \x1bT/\x1bE tags as well.
 *
 * This representation is chosen so that clients ignoring escape codes will
 * see untranslated strings.
 *
 * For instance, suppose we have a string such as "@1 Wool" with the argument "White"
 * The string will be sent as "\x1bT\x1bF\x1bTWhite\x1bE\x1bE Wool\x1bE"
 * To translate this string, we extract what is inside \x1bT/\x1bE tags.
 * When we notice the \x1bF tag, we recursively extract what is there up to the \x1bE end tag,
 * translating it as well.
 * We get the argument "White", translated, and create a template string with "@1" instead of it.
 * We finally get the template "@1 Wool" that was used in the beginning, which we translate
 * before filling it again.
 */

void translate_all(const std::wstring &s, size_t &i,
		Translations *translations, std::wstring &res);

void translate_string(const std::wstring &s, Translations *translations,
		const std::wstring &textdomain, size_t &i, std::wstring &res)
{
	std::wostringstream output;
	std::vector<std::wstring> args;
	int arg_number = 1;
	while (i < s.length()) {
		// Not an escape sequence: just add the character.
		if (s[i] != '\x1b') {
			output.put(s[i]);
			// The character is a literal '@'; add it twice
			// so that it is not mistaken for an argument.
			if (s[i] == L'@')
				output.put(L'@');
			++i;
			continue;
		}

		// We have an escape sequence: locate it and its data
		// It is either a single character, or it begins with '('
		// and extends up to the following ')', with '\' as an escape character.
		++i;
		size_t start_index = i;
		size_t length;
		if (i == s.length()) {
			length = 0;
		} else if (s[i] == L'(') {
			++i;
			++start_index;
			while (i < s.length() && s[i] != L')') {
				if (s[i] == L'\\')
					++i;
				++i;
			}
			length = i - start_index;
			++i;
			if (i > s.length())
				i = s.length();
		} else {
			++i;
			length = 1;
		}
		std::wstring escape_sequence(s, start_index, length);

		// The escape sequence is now reconstructed.
		std::vector<std::wstring> parts = split(escape_sequence, L'@');
		if (parts[0] == L"E") {
			// "End of translation" escape sequence. We are done locating the string to translate.
			break;
		} else if (parts[0] == L"F") {
			// "Start of argument" escape sequence.
			// Recursively translate the argument, and add it to the argument list.
			// Add an "@n" instead of the argument to the template to translate.
			if (arg_number >= 10) {
				errorstream << "Ignoring too many arguments to translation" << std::endl;
				std::wstring arg;
				translate_all(s, i, translations, arg);
				args.push_back(arg);
				continue;
			}
			output.put(L'@');
			output << arg_number;
			++arg_number;
			std::wstring arg;
			translate_all(s, i, translations, arg);
			args.push_back(arg);
		} else {
			// This is an escape sequence *inside* the template string to translate itself.
			// This should not happen, show an error message.
			errorstream << "Ignoring escape sequence '" << wide_to_narrow(escape_sequence) << "' in translation" << std::endl;
		}
	}

	std::wstring toutput;
	// Translate the template.
	if (translations != nullptr)
		toutput = translations->getTranslation(
				textdomain, output.str());
	else
		toutput = output.str();

	// Put back the arguments in the translated template.
	std::wostringstream result;
	size_t j = 0;
	while (j < toutput.length()) {
		// Normal character, add it to output and continue.
		if (toutput[j] != L'@' || j == toutput.length() - 1) {
			result.put(toutput[j]);
			++j;
			continue;
		}

		++j;
		// Literal escape for '@'.
		if (toutput[j] == L'@') {
			result.put(L'@');
			++j;
			continue;
		}

		// Here we have an argument; get its index and add the translated argument to the output.
		int arg_index = toutput[j] - L'1';
		++j;
		if (0 <= arg_index && (size_t)arg_index < args.size()) {
			result << args[arg_index];
		} else {
			// This is not allowed: show an error message
			errorstream << "Ignoring out-of-bounds argument escape sequence in translation" << std::endl;
		}
	}
	res = result.str();
}

void translate_all(const std::wstring &s, size_t &i,
		Translations *translations, std::wstring &res)
{
	std::wostringstream output;
	while (i < s.length()) {
		// Not an escape sequence: just add the character.
		if (s[i] != '\x1b') {
			output.put(s[i]);
			++i;
			continue;
		}

		// We have an escape sequence: locate it and its data
		// It is either a single character, or it begins with '('
		// and extends up to the following ')', with '\' as an escape character.
		size_t escape_start = i;
		++i;
		size_t start_index = i;
		size_t length;
		if (i == s.length()) {
			length = 0;
		} else if (s[i] == L'(') {
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
			if (i > s.length())
				i = s.length();
		} else {
			++i;
			length = 1;
		}
		std::wstring escape_sequence(s, start_index, length);

		// The escape sequence is now reconstructed.
		std::vector<std::wstring> parts = split(escape_sequence, L'@');
		if (parts[0] == L"E") {
			// "End of argument" escape sequence. Exit.
			break;
		} else if (parts[0] == L"T") {
			// Beginning of translated string.
			std::wstring textdomain;
			if (parts.size() > 1)
				textdomain = parts[1];
			std::wstring translated;
			translate_string(s, translations, textdomain, i, translated);
			output << translated;
		} else {
			// Another escape sequence, such as colors. Preserve it.
			output << std::wstring(s, escape_start, i - escape_start);
		}
	}

	res = output.str();
}

// Translate string server side
std::wstring translate_string(const std::wstring &s, Translations *translations)
{
	size_t i = 0;
	std::wstring res;
	translate_all(s, i, translations, res);
	return res;
}

// Translate string client side
std::wstring translate_string(const std::wstring &s)
{
#ifdef SERVER
	return translate_string(s, nullptr);
#else
	return translate_string(s, g_client_translations);
#endif
}

// >> KIDSCODE
void fix_accented_characters(std::wstring &s,
	std::vector<irr::video::SColor> *colors) {
	int l = s.size();
	int nl = 0;
	for (int i = 0; i < l; ++i, ++nl) {
		if (colors)
			(*colors)[nl] = (*colors)[i];

		unsigned c = s[i];
		if (c >= 0xc3 && i+1 < l) {
			unsigned nc = 0;
			unsigned c2 = s[i+1];
			if (c2 >= 0x80 && c2 <= 0xBF) {
				// lowercase letters
				nc = c2 + 0x40;
			} else {
				// uppercase letters
				switch (c2) {
					case 0x20AC : nc = 0xC0; break; // À
					case 0x201A : nc = 0xC2; break; // Â
					case 0x192 : nc = 0xC3; break; // Ã
					case 0x201E : nc = 0xC4; break; // Ä
					case 0x2026 : nc = 0xC5; break; // Å
					case 0x2020 : nc = 0xC6; break; // Æ
					case 0x2021 : nc = 0xC7; break; // Ç
					case 0x2C6 : nc = 0xC8; break; // È
					case 0x2030 : nc = 0xC9; break; // É
					case 0x160 : nc = 0xCA; break; // Ê
					case 0x2039 : nc = 0xCB; break; // Ë
					case 0x152 : nc = 0xCC; break; // Ì
					case 0x17D : nc = 0xCE; break; // Î
					case 0x2018 : nc = 0xD1; break; // Ñ
					case 0x2019 : nc = 0xD2; break; // Ò
					case 0x201C : nc = 0xD3; break; // Ó
					case 0x201D : nc = 0xD4; break; // Ô
					case 0x2022 : nc = 0xD5; break; // Õ
					case 0x2013 : nc = 0xD6; break; // Ö
					case 0x2122 : nc = 0xD9; break; // Ù
					case 0x161 : nc = 0xDA; break; // Ú
					case 0x203A : nc = 0xDB; break; // Û
					case 0x153 : nc = 0xDC; break; // Ü
				}
			}
			if (nc) {
				s[nl]=nc;
				++i;
			}
			else {
				s[nl]=c;
			}
		} else {
			s[nl]=c;
		}
	}

	s.resize(nl);

	if (colors)
		colors->resize(nl);
}

std::wstring fix_string(const std::wstring &s) {
	std::wstring res(s);
	fix_accented_characters(res, 0);
	return res;
}
// << KIDSCODE
