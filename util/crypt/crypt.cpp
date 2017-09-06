// :PATCH:

#include "encryption.h"
#include <cstddef>
#include <iostream>
#include <string.h>
#include <stdio.h>

char * readText(const char *path, size_t &size)
{
	FILE *file = fopen(path, "rb");
	if (!file)
		return NULL;

	if (fseek(file, 0, SEEK_END)) {
		fclose(file);
		return NULL;
	}

	size = ftell(file);
	char *text = new char[size];
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
	if (written_size != size) {
		return false;
	}
	
	return text;
}

bool processFile(const char *path, const char option)
{
	bool success = false;
	char *text;
	size_t size;
	
	if (option == 'e') {
		std::cout << "Encrypting file : " << path << std::endl;

		text = readText(path, size);
		
		if (text == NULL) 
			return false;
			
		const char *encrypted_text = encryptText(text, size, path);
		
		if (encrypted_text != text)
			success = writeText(encrypted_text, size, path);
			
		delete[] encrypted_text;
	}
	else if (option == 'd') {
		std::cout << "Decrypting file : " << path << std::endl;
		
		text = readText(path, size);
		
		if (text == NULL)
			return false;
			
		const char *decrypted_text = decryptText(text, size, path);
		
		if (decrypted_text != text)
			success = writeText(decrypted_text, size, path);
			
		delete[] decrypted_text;
	}
	
	return success;
}


int main(int argc, char** argv)
{
	char option = 0;
	
	--argc;
	++argv;
	
	if (argc > 0 && (!strcmp(argv[0], "-e") || !strcmp(argv[0], "-d"))) {
		option = argv[0][1];
		--argc;
		++argv;
	}
	
	if (option) {		
		for (int i = 0; i < argc; ++i) {
			if (!processFile(argv[i], option)) {
				std::cout << "*** ERROR : can't process file : " << argv[i] << "\n";
			}
		}
	}
	else {
		std::cout << "*** ERROR : missing option\n";
		std::cout << "Usage :\n";
		std::cout << "        crypt option file1.lua file2.lua ...\n";
		std::cout << "Options :\n";
		std::cout << "        -e : encrypt files\n";
		std::cout << "        -d : decrypt files\n";
	}
}


