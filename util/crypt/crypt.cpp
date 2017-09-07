// :PATCH:

#include <cstddef>
#include <iostream>
#include <string.h>
#include <stdio.h>

#include "encryption.h"

char *readText(const char *path, size_t &size)
{
	printf("Reading file : %s\n", path);

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
	printf("Writing file : %s\n", path);

	FILE *file = fopen(path, "wb");
	if (!file)
		return false;

	size_t written_size = fwrite(text, 1, size, file);
	fclose(file);

	if (written_size != size)
		return false;

	return true;
}


bool processFile(const char *path, const char option)
{
	bool success = false;
	char *text;
	size_t size;

	if (option == 'e') {
		printf("Encrypting file : %s\n", path);

		text = readText(path, size);

		if (text == NULL)
			return false;

		const char *encrypted_text = encryptText(text, size, path);

		if (encrypted_text != text)
			success = writeText(encrypted_text, size, path);

		delete[] encrypted_text;
	}
	else if (option == 'd') {
		printf( "Decrypting file : %s\n", path);

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


void processFiles(int count, char **paths, char option)
{
	if ( count == 0 ) {
		size_t size;
		char *list = readText("CryptList.txt", size);
		
		if (list == NULL) {
			puts("*** ERROR : can't read CryptList.txt");
			return;
		}
		
		for (std::string path; *list; ++list) {
			if (*list=='\n') {
				if (path.size() > 0)
					processFile(path.c_str(),option);
				path = "";
			}
			else
				path += *list;
		}
	}
	else {
		for (int i = 0; i < count; ++i) {
			if (!processFile(paths[i], option)) {
				printf("*** ERROR : can't process file : %s\n", paths[i]);
			}
		}
	}
}


int main(int argc, char **argv)
{
	char option = 0;

	--argc;
	++argv;

	if (argc > 0 && (!strcmp(argv[0], "-e") || !strcmp(argv[0], "-d"))) {
		option = argv[0][1];
		--argc;
		++argv;
	}

	if (option)
		processFiles(argc, argv, option);
	else {
		puts("*** ERROR : missing option");
		puts("Usage :");
		puts("        crypt -e file1 file2 ... : encrypt these files");
		puts("        crypt -d file1 file2 ... : decrypt these files");
		puts("        crypt -e : encrypt the files specified in CryptList.txt");
		puts("        crypt -d : decrypt the files specified in CryptList.txt");
	}
}


