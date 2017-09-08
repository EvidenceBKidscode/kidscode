// :PATCH:

#include <cstddef>
#include <iostream>
#include <string.h>
#include <stdio.h>

#include "encryption.h"

bool processFile(const char *path, const char option)
{
	bool success = false;
	char *text;
	size_t size;

	if (option == 'e') {
		text = readText(path, size);

		if (text == NULL)
			return false;

		const char *encrypted_text = encryptText(text, size, path);

		if (encrypted_text != text) {
			printf("Encrypting file : %s\n", path);
			success = writeText(encrypted_text, size, path);
		}

		delete[] encrypted_text;
	}
	else if (option == 'd') {
		text = readText(path, size);

		if (text == NULL)
			return false;

		const char *decrypted_text = decryptText(text, size, path);

		if (decrypted_text != text) {
			printf("Decrypting file : %s\n", path);
			success = writeText(decrypted_text, size, path);
		}

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
			if (*list == '\n') {
				if (path.size() > 0)
					processFile(path.c_str(),option);
				path = "";
			}
			else if (*list != '\r')
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


