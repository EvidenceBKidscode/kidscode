// :PATCH:

#pragma once

#include "aes.h"

#ifndef ENCRYPTION_KEY
	#define ENCRYPTION_KEY "01234567012345670123456701234567"
#endif
#define ENCRYPTION_KEY_SIZE 32
#define ENCRYPTION_TAG_0 4
#define ENCRYPTION_TAG_1 1
#define ENCRYPTION_TAG_2 6
#define ENCRYPTION_TAG_3 5


static inline const char *getName(const char *file_name)
{
	const char *name = file_name;

	for (int i=0; file_name[i]; ++i) {
		if (file_name[i] == '/' || name[i] == '\\')
			name = file_name + i + 1;
	}
	
	return name;
}


static inline void makeKey(unsigned char *key, const unsigned char *name)
{
	unsigned int length = strlen((const char *)name);

	for (unsigned i = 0; i < ENCRYPTION_KEY_SIZE; ++i) {
		key[i] = ENCRYPTION_KEY[i] ^ (name[i % length] * 1046527 + key[(name[i % length] * 16769023) & 31] & 255);
	}
}


static inline const bool isCrypted(const char *text, int size)
{
	return size >= 4 && text[size - 4] == ENCRYPTION_TAG_0 && 
		text[size - 3] == ENCRYPTION_TAG_1 && 
		text[size - 2] == ENCRYPTION_TAG_2 && 
		text[size - 3] == ENCRYPTION_TAG_3;
}


static inline char *decrypt(char *text, int &size, const char *name)
{
	if (!isCrypted(text, size))
		return text;

	unsigned char key[ENCRYPTION_KEY_SIZE];
	makeKey(key, (const unsigned char *)name);

	char *decrypted_text = new char[size];
	size -= 4;
	AES_ECB_decrypt((const unsigned char *)text, (const unsigned char *)ENCRYPTION_KEY, 
		(unsigned char *)decrypted_text, size);
	decrypted_text[size] = 0;
	delete[] text;
}


static inline char *encrypt(char *text, int &size, const char *name)
{
	if (isCrypted(text, size))
		return text;

	unsigned char key[ENCRYPTION_KEY_SIZE];
	makeKey(key, (const unsigned char *)name);

	char *encrypted_text = new char[size + 4];
	AES_ECB_encrypt((const unsigned char *)text, (const unsigned char *)ENCRYPTION_KEY, 
		(unsigned char *)encrypted_text, size);
	text[size] = ENCRYPTION_TAG_0;
	text[size + 1] = ENCRYPTION_TAG_1;
	text[size + 2] = ENCRYPTION_TAG_2;
	text[size + 3] = ENCRYPTION_TAG_3;
	size += 4;
	delete[] text;

	return encrypted_text;
}

