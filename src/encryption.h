// :PATCH:

#pragma once

#include "aes.h"


#define ENCRYPTION_KEY "01234567012345670123456701234567"
#define ENCRYPTION_KEY_SIZE 32
#define ENCRYPTION_KEY_PRIME_0 1046527
#define ENCRYPTION_KEY_PRIME_1 2654435761
#define ENCRYPTION_BUFFER_SIZE 16
#define ENCRYPTION_TAG_0 4
#define ENCRYPTION_TAG_1 1
#define ENCRYPTION_TAG_2 6
#define ENCRYPTION_TAG_3 5


static inline const char *getFileName(const char *file_path)
{
	const char *file_name = file_path;

	for (int i=0; file_path[i]; ++i) {
		if (file_path[i] == '/' || file_path[i] == '\\')
			file_name = file_path + i + 1;
	}
	
	printf( "NAME: %s\n", file_name );
	return file_name;
}


static inline void makeKey(unsigned char *key, const char *file_path)
{
	const unsigned char *name = (const unsigned char *)getFileName(file_path);
	unsigned int length = strlen((const char *)name);

	for (unsigned i = 0; i < ENCRYPTION_KEY_SIZE; ++i) {
		key[i] = ENCRYPTION_KEY[i] * name[i % length] * ENCRYPTION_KEY_PRIME_0 + 
			i * ENCRYPTION_KEY_PRIME_1;
		printf("%d ", unsigned(key[i]));
	}
	puts("\n");
}


static inline const bool isCryptedText(const char *text, size_t size)
{
	return size >= 4 && text[0] == ENCRYPTION_TAG_0 && text[1] == ENCRYPTION_TAG_1 && 
		text[2] == ENCRYPTION_TAG_2 && text[3] == ENCRYPTION_TAG_3;
}


static inline char *encryptText(char *text, size_t &size, const char *file_path)
{
	if (isCryptedText(text, size))
		return text;

	char *encrypted_text = new char[size + 4];
	
	encrypted_text[0] = ENCRYPTION_TAG_0;
	encrypted_text[1] = ENCRYPTION_TAG_1;
	encrypted_text[2] = ENCRYPTION_TAG_2;
	encrypted_text[3] = ENCRYPTION_TAG_3;

	unsigned char key[ENCRYPTION_KEY_SIZE];
	makeKey(key, file_path);

	aes256_context ctx; 
	aes256_init(&ctx, key);
	
	unsigned char buffer[ENCRYPTION_BUFFER_SIZE];

	for (unsigned i = 0; i < size; i += ENCRYPTION_BUFFER_SIZE) {
		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j)
			buffer[j] = (i + j < size) ? text[i + j] : 0;
			
		aes256_encrypt_ecb(&ctx, buffer);

		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			if (i + j < size)
				encrypted_text[4 + i + j] = buffer[j];
		}
	}
	
	aes256_done(&ctx);

	size += 4;
	delete[] text;

	return encrypted_text;
}


static inline char *decryptText(char *text, size_t &size, const char *file_path)
{
	if (!isCryptedText(text, size))
		return text;
		
	size -= 4;
	char *decrypted_text = new char[size + 1];
	decrypted_text[size] = 0;
	
	unsigned char key[ENCRYPTION_KEY_SIZE];
	makeKey(key, file_path);

	aes256_context ctx; 
	aes256_init(&ctx, key);
	
	unsigned char buffer[ENCRYPTION_BUFFER_SIZE];

	for (unsigned i = 0; i < size; i += ENCRYPTION_BUFFER_SIZE) {
		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j)
			buffer[j] = (i + j < size) ? text[4 + i + j] : 0;
			
		aes256_decrypt_ecb(&ctx, buffer);

		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			if (i + j < size)
				decrypted_text[i + j] = buffer[j];
		}
	}
	
	aes256_done(&ctx);

	delete[] text;
	
	return decrypted_text;
}
