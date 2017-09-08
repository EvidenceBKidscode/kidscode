// :PATCH:

#pragma once

#include "aes.h"
#include "encryption_data.h"


#define ENCRYPTION_TAG 0x04030501
#define ENCRYPTION_HEADER_SIZE 16
#define ENCRYPTION_KEY_SIZE 8
#define ENCRYPTION_BUFFER_SIZE 16


char *readText(const char *path, size_t &size)
{
	FILE *file = fopen(path, "rb");
	if (!file)
		return NULL;

	if (fseek(file, 0, SEEK_END)) {
		fclose(file);
		return NULL;
	}

	size = ftell(file);
	char *text = new char[size + 1];
	text[size] = 0;

	if (fseek(file, 0, SEEK_SET)) {
		fclose(file);
		delete [] text;
		return NULL;
	}

	size_t read_size = fread(text, 1, size, file);
	fclose(file);

	if (read_size != size) {
		delete [] text;
		return NULL;
	}

	return text;
}


bool writeText(const char *text, const size_t size, const char *path)
{
	FILE *file = fopen(path, "wb");
	if (!file)
		return false;

	size_t written_size = fwrite(text, 1, size, file);
	fclose(file);

	if (written_size != size)
		return false;

	return true;
}


static inline const char *getFileName(const char *file_path)
{
	const char *file_name = file_path;

	for (int i=0; file_path[i]; ++i) {
		if (file_path[i] == '/' || file_path[i] == '\\')
			file_name = file_path + i + 1;
	}

	return file_name;
}


static inline void makeKey(unsigned *key, const char *file_name)
{
	unsigned int file_name_length = strlen(file_name);

	key[0] = ENCRYPTION_KEY_0;
	key[1] = ENCRYPTION_KEY_1;
	key[2] = ENCRYPTION_KEY_2;
	key[3] = ENCRYPTION_KEY_3;
	key[4] = ENCRYPTION_KEY_4;
	key[5] = ENCRYPTION_KEY_5;
	key[6] = ENCRYPTION_KEY_6;
	key[7] = ENCRYPTION_KEY_7;
	
	for (unsigned i = 0; i < file_name_length; ++i) {
		key[i % ENCRYPTION_KEY_SIZE] += file_name[i];
	}
	
	key[0] = key[0] + ENCRYPTION_PRIME_7;
	key[1] = (key[1] ^ key[0] * ENCRYPTION_PRIME_1) + ENCRYPTION_PRIME_0;
	key[2] = (key[2] ^ key[1] * ENCRYPTION_PRIME_2) + ENCRYPTION_PRIME_1;
	key[3] = (key[3] ^ key[2] * ENCRYPTION_PRIME_3) + ENCRYPTION_PRIME_2;
	key[4] = (key[4] ^ key[3] * ENCRYPTION_PRIME_4) + ENCRYPTION_PRIME_3;
	key[5] = (key[5] ^ key[4] * ENCRYPTION_PRIME_5) + ENCRYPTION_PRIME_4;
	key[6] = (key[6] ^ key[5] * ENCRYPTION_PRIME_6) + ENCRYPTION_PRIME_5;
	key[7] = (key[7] ^ key[6] * ENCRYPTION_PRIME_7) + ENCRYPTION_PRIME_6;
}


static inline const bool isCryptedText(const char *text, size_t size)
{
	return size >= 4 && *((unsigned *)text) == ENCRYPTION_TAG;
}


static inline const char *encryptText(const char *text, size_t &size, const char *file_path)
{
	if (isCryptedText(text, size))
		return text;

	const char *file_name = getFileName(file_path);

	unsigned decrypted_size = size;
	unsigned encrypted_size = (((decrypted_size + ENCRYPTION_BUFFER_SIZE - 1) / 
				ENCRYPTION_BUFFER_SIZE) * ENCRYPTION_BUFFER_SIZE);
	unsigned file_name_size = strlen(file_name) + 1;

	size = ENCRYPTION_HEADER_SIZE + encrypted_size + file_name_size;
		
	char *encrypted_text = new char[size];

	*((unsigned *)encrypted_text) = ENCRYPTION_TAG;
	((unsigned *)encrypted_text)[1] = decrypted_size;
	((unsigned *)encrypted_text)[2] = encrypted_size;
	((unsigned *)encrypted_text)[3] = file_name_size;
	memcpy(encrypted_text + ENCRYPTION_HEADER_SIZE + encrypted_size, file_name, file_name_size);

	unsigned key[ENCRYPTION_KEY_SIZE];
	makeKey(key, file_name);

	unsigned char *input = (unsigned char *)text;
	unsigned char *output = (unsigned char *)(encrypted_text + ENCRYPTION_HEADER_SIZE);
	unsigned char buffer[ENCRYPTION_BUFFER_SIZE];

	aes256_context ctx;
	aes256_init(&ctx, (unsigned char *)key);
	
	for (unsigned i = 0; i < encrypted_size; i += ENCRYPTION_BUFFER_SIZE) {
		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			buffer[j] = (i + j < decrypted_size) ? input[i + j] : 0;
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


static inline const char *decryptText(const char *text, size_t &size, const char *file_path)
{
	if (!isCryptedText(text, size))
		return text;

	unsigned decrypted_size = ((unsigned *)text)[ 1 ];
	unsigned encrypted_size = ((unsigned *)text)[ 2 ];
	
	const char *file_name = text + ENCRYPTION_HEADER_SIZE + encrypted_size;

	size = decrypted_size;

	char *decrypted_text = new char[size + 1];
	decrypted_text[size] = 0;

	unsigned key[ENCRYPTION_KEY_SIZE];
	makeKey(key, file_name);

	unsigned char *input = (unsigned char *)(text + ENCRYPTION_HEADER_SIZE);
	unsigned char *output = (unsigned char *)decrypted_text;
	unsigned char buffer[ENCRYPTION_BUFFER_SIZE];

	aes256_context ctx;
	aes256_init(&ctx, (unsigned char *)key);

	for (unsigned i = 0; i < encrypted_size; i += ENCRYPTION_BUFFER_SIZE) {
		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			buffer[j] = input[i + j];
		}

		aes256_decrypt_ecb(&ctx, buffer);

		for (unsigned j = 0; j < ENCRYPTION_BUFFER_SIZE; ++j) {
			if (i + j < decrypted_size)
				output[i + j] = buffer[j];
		}
	}

	aes256_done(&ctx);

	delete[] text;

	return decrypted_text;
}
