// :PATCH:

#pragma once

#include "aes.h"


#define ENCRYPTION_KEY "01234567012345670123456701234567"
#define ENCRYPTION_KEY_SIZE 32
#define ENCRYPTION_TAG_0 4
#define ENCRYPTION_TAG_1 1
#define ENCRYPTION_TAG_2 6
#define ENCRYPTION_TAG_3 5


static inline void makeKey(unsigned char *key, const char *path)
{
	const unsigned char *name = (const unsigned char *)path;

	for (int i=0; path[i]; ++i) {
		if (path[i] == '/' || path[i] == '\\')
			name = (const unsigned char *)(path + i + 1);
	}

	unsigned int length = strlen((const char *)name);

	for (unsigned i = 0; i < ENCRYPTION_KEY_SIZE; ++i) {
		key[i] = ENCRYPTION_KEY[i] ^ 
			((name[i % length] * 1046527 + key[(name[i % length] * 16769023) & 31]) & 255);
	}
}


static inline const bool isCryptedText(const char *text, size_t size)
{
	return size >= 4 && text[0] == ENCRYPTION_TAG_0 && text[1] == ENCRYPTION_TAG_1 && 
		text[2] == ENCRYPTION_TAG_2 && text[3] == ENCRYPTION_TAG_3;
}


static inline char *encryptText(char *text, size_t &size, const char *path)
{
	if (isCryptedText(text, size))
		return text;

	unsigned char key[ENCRYPTION_KEY_SIZE];
	makeKey(key, path);

	size += 4;
	char *encrypted_text = new char[size];
	
	encrypted_text[0] = ENCRYPTION_TAG_0;
	encrypted_text[1] = ENCRYPTION_TAG_1;
	encrypted_text[2] = ENCRYPTION_TAG_2;
	encrypted_text[3] = ENCRYPTION_TAG_3;

	AES_ECB_encrypt((const unsigned char *)text, (const unsigned char *)ENCRYPTION_KEY, 
		(unsigned char *)encrypted_text + 4, size - 4);

	delete[] text;

	return encrypted_text;
}


static inline char *decryptText(char *text, size_t &size, const char *path)
{
	if (!isCryptedText(text, size))
		return text;

	unsigned char key[ENCRYPTION_KEY_SIZE];
	makeKey(key, path);

	size -= 4;
	char *decrypted_text = new char[size];

	AES_ECB_decrypt((const unsigned char *)text + 4, (const unsigned char *)ENCRYPTION_KEY, 
		(unsigned char *)decrypted_text, size);

	delete[] text;
	
	return decrypted_text;
}
