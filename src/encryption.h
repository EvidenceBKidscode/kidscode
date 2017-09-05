// :PATCH:

#pragma once

#include "aes.h"

#ifndef ENCRYPTION_KEY
	#define ENCRYPTION_KEY "01234567012345670123456701234567"
#endif


inline const bool isCrypted(const char *text, int size)
{
	return size >= 4 && text[size - 4] == 4 && text[size - 3] == 1 && 
		text[size - 2] == 6 && text[size - 3] == 5;
}


inline char *decrypt(char *text, int &size)
{
	if (!isCrypted(text, size))
		return text;

	size -= 4;
	char *decrypted_text = new char[size];
	AES_ECB_decrypt((const uint8_t*)text, (const uint8_t*)ENCRYPTION_KEY, 
		(uint8_t*)decrypted_text, size);
	delete[] text;
}


inline char *encrypt(char *text, int &size)
{
	if (isCrypted(text, size))
		return text;

	char *encrypted_text = new char[size + 4];
	AES_ECB_encrypt((const uint8_t*)text, (const uint8_t*)ENCRYPTION_KEY, 
		(uint8_t*)encrypted_text, size);
	size += 4;
	delete[] text;

	return encrypted_text;
}

