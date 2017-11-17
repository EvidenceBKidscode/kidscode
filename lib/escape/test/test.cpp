#include "escape.h"
#include <iostream>
#include <cassert>

int main()
{
	std::string name = "abcdefghijklmnopqrstuvwxyzáâäàãåæçéêëèíîìïóôöòõúûùüýÿñABCDEFGHIJKLMNOPQRSTUVWXYZÑÁÂÀÃÄÅÆÇÉÊËÈÍÎÏÌÓÔÒÖÕÚÛÜÙÝ0123456789-_ +-;@\\";
	std::cout << name << std::endl;
	
	std::string encoded_name = getEncodedName(name);
	std::cout << encoded_name << std::endl;
	
	std::string decoded_name = getDecodedName(encoded_name);
	std::cout << decoded_name << std::endl;
	
	assert(decoded_name == name);
	
	return 0;
}
