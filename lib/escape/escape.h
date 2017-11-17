#ifndef __ESCAPE_H__
#define __ESCAPE_H__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cctype>

#define ESCAPE_CHAR '\\'

std::string getEncodedName(std::string name)
{
	size_t l = name.length();
	const char *str = name.c_str();
	char buf[4];
	std::string encoded_name;
	
	for (unsigned i=0; i < l; ++i)
	{
		unsigned char c = str[i];
		
		if (isalnum(c))
			encoded_name += (char)c;
		else
		{
			sprintf(buf,"%03u",(unsigned)c);
			encoded_name += ESCAPE_CHAR;
			encoded_name += buf;
		}
	}
	
	return encoded_name;
}

std::string getDecodedName(std::string name)
{
	size_t l = name.length();
	const char *str = name.c_str();
	char buf[4];
	buf[3]=0;
	std::string decoded_name;
	
	for (unsigned i=0; i < l; ++i)
	{
		unsigned char c = str[i];
		
		if (c == ESCAPE_CHAR && i + 3 < l)
		{
			strncpy(buf, str + i + 1, 3);
			i += 3;
			c = atoi(buf);
		}
		
		decoded_name += (char)c;
	}
	
	return decoded_name;
}
#endif
