// :PATCH:

#pragma once

#include "aes.h"

#ifndef ENCRYPTION_KEY
	#define ENCRYPTION_KEY "01234567012345670123456701234567"
#endif

#define ENCRYPTION_TAG_0 4
#define ENCRYPTION_TAG_1 1
#define ENCRYPTION_TAG_2 6
#define ENCRYPTION_TAG_3 5

inline const bool isCrypted(const char *text, int size)
{
	return size >= 4 && text[size - 4] == ENCRYPTION_TAG_0 && 
		text[size - 3] == ENCRYPTION_TAG_1 && 
		text[size - 2] == ENCRYPTION_TAG_2 && 
		text[size - 3] == ENCRYPTION_TAG_3;
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
	text[size] = ENCRYPTION_TAG_0;
	text[size + 1] = ENCRYPTION_TAG_1;
	text[size + 2] = ENCRYPTION_TAG_2;
	text[size + 3] = ENCRYPTION_TAG_3;
	size += 4;
	delete[] text;

	return encrypted_text;
}

