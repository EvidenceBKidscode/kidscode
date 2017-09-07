// :PATCH:

#pragma once

#include "aes.h"


#define ENCRYPTION_TAG 0x04030501
#define ENCRYPTION_HEADER_SIZE 8
#define ENCRYPTION_KEY_SIZE 8
#define ENCRYPTION_BUFFER_SIZE 16

#ifdef ENCRYPTION_DATA
	#include "encryption_data.h"
#else
	#define ENCRYPTION_KEY_0 0x01234567
	#define ENCRYPTION_KEY_1 0x01234567
	#define ENCRYPTION_KEY_2 0x01234567
	#define ENCRYPTION_KEY_3 0x01234567
	#define ENCRYPTION_KEY_4 0x01234567
	#define ENCRYPTION_KEY_5 0x01234567
	#define ENCRYPTION_KEY_6 0x01234567
	#define ENCRYPTION_KEY_7 0x01234567
	#define ENCRYPTION_PRIME 0x01234567
#endif


static inline const char *getFileName(const char *file_path)
{
	const char *file_name = file_path;

	for (int i=0; file_path[i]; ++i) {
		if (file_path[i] == '/' || file_path[i] == '\\')
			file_name = file_path + i + 1;
	}

	return file_name;
}


static inline void makeKey(unsigned *key, const char *file_path)
{
	const unsigned char *name = (const unsigned char *)getFileName(file_path);
	unsigned int length = strlen((const char *)name);

	key[0] = ENCRYPTION_KEY_0 ^ ENCRYPTION_PRIME;
	key[1] = ENCRYPTION_KEY_1 ^ key[0] * ENCRYPTION_PRIME;
	key[2] = ENCRYPTION_KEY_2 ^ key[1] * ENCRYPTION_PRIME;
	key[3] = ENCRYPTION_KEY_3 ^ key[2] * ENCRYPTION_PRIME;
	key[4] = ENCRYPTION_KEY_4 ^ key[3] * ENCRYPTION_PRIME;
	key[5] = ENCRYPTION_KEY_5 ^ key[4] * ENCRYPTION_PRIME;
	key[6] = ENCRYPTION_KEY_6 ^ key[5] * ENCRYPTION_PRIME;
	key[7] = ENCRYPTION_KEY_7 ^ key[6] * ENCRYPTION_PRIME;
	
	for (unsigned i = 0; i < length; ++i) {
		unsigned k = i % ENCRYPTION_KEY_SIZE;
		key[k] *= i + key[k] * name[i] * ENCRYPTION_PRIME;
	}
}


static inline const bool isCryptedText(const char *text, size_t size)
{
	return size >= 4 && *((unsigned *)text) == ENCRYPTION_TAG;
}


static inline char *encryptText(char *text, size_t &size, const char *file_path)
{
	if (isCryptedText(text, size))
		return text;

	unsigned length = size;

	size = ENCRYPTION_HEADER_SIZE + (((length + ENCRYPTION_BUFFER_SIZE - 1) >> 4) << 4);
	char *encrypted_text = new char[size];

	*((unsigned *)encrypted_text) = ENCRYPTION_TAG;
	((unsigned *)encrypted_text)[ 1 ] = length;

	unsigned key[ENCRYPTION_KEY_SIZE];
	makeKey(key, file_path);

	aes256_context ctx;
	aes256_init(&ctx, (unsigned char *)key);

	unsigned char *input = (unsigned char *)text;
	unsigned char *output = (unsigned char *)(encrypted_text + ENCRYPTION_HEADER_SIZE);
	unsigned char buffer[ENCRYPTION_BUFFER_SIZE];

	for (unsigned i = 0; i < length; i += ENCRYPTION_BUFFER_SIZE) {
		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			buffer[j] = (i + j < length) ? input[i + j] : 0;
		}

		aes256_encrypt_ecb(&ctx, buffer);

		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			output[i + j] = buffer[j];
		}
	}

	aes256_done(&ctx);

	delete[] text;

	return encrypted_text;
}


static inline char *decryptText(char *text, size_t &size, const char *file_path)
{
	if (!isCryptedText(text, size))
		return text;

	size = ((unsigned *)text)[ 1 ];

	char *decrypted_text = new char[size + 1];
	decrypted_text[size] = 0;

	unsigned key[ENCRYPTION_KEY_SIZE];
	makeKey(key, file_path);

	aes256_context ctx;
	aes256_init(&ctx, (unsigned char *)key);

	unsigned length = size;
	unsigned char *input = (unsigned char *)(text + ENCRYPTION_HEADER_SIZE);
	unsigned char *output = (unsigned char *)decrypted_text;
	unsigned char buffer[ENCRYPTION_BUFFER_SIZE];

	for (unsigned i = 0; i < length; i += ENCRYPTION_BUFFER_SIZE) {
		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			buffer[j] = input[i + j];
		}

		aes256_decrypt_ecb(&ctx, buffer);

		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			if (i + j < length)
				output[i + j] = buffer[j];
		}
	}

	aes256_done(&ctx);

	delete[] text;

	return decrypted_text;
}
