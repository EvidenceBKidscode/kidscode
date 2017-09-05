// :PATCH:

#include "aes.cpp"
#include "encryption.h"
#include <iostream>
#include <string>
#include <stdio.h>


void processFile(const char *file_path, const char mode)
{
	std::cout << file_path << std::endl;

	
}


int main(int argc, char** argv)
{
	char mode = 'e';
	
	--argc;
	++argv;
	
	if (argc > 0 && *argv[0] == '-') {
		mode = argv[0][1];
		--argc;
		++argv;
	}
	
	for (int i = 0; i < argc; ++i) {
		processFile(argv[i], mode);
	}
}


