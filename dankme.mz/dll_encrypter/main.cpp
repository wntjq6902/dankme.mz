#include "Rijndael.h"

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <limits>
#include <fstream>
#include <sstream>

void encrypt(char *adr, int len, long long key)
{
	for (unsigned i = 0; i < len; i++)
	{
		adr[i] ^= key;
		adr[i] ^= ~key;
	}
}
void decrypt(char *adr, int len, long long key)
{
	for (unsigned i = 0; i < len; i++)
	{
		adr[i] ^= ~key;
		adr[i] ^= key;
	}
}

int main()
{
	std::string inputstr;
	std::ifstream input;

	while (!input.good() || !input.is_open())
	{
		std::getline(std::cin, inputstr);
		input.open(inputstr, std::ios::binary);
		if (!input.good())
		{
			std::cout << "Bad filename!\n";
		}
	}

	input.seekg(0, std::ios::end);						//gets file size
	int size = input.tellg();
	input.seekg(0, std::ios::beg);
	size -= input.tellg();

	char *data = new char[size];

	input.read(data, size);

	long long encryption_key = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() * (int)((long)__DATE__ ^ (int)__TIME__);
	long long encryption_key_flip_shift = ~encryption_key >> 69; //meme

	std::ostringstream key_stream, key_flip_shift_steam;
	key_stream << encryption_key; key_flip_shift_steam << encryption_key_flip_shift;

	CRijndael const_key, dynamic_key;

	const_key.MakeKey("i-pLAy-pOkeMOn-gO-eVErYdaY", CRijndael::sm_chain0, 32, 16);	//b1g meme
	dynamic_key.MakeKey(key_stream.str().c_str(), key_flip_shift_steam.str().c_str(), 32, 16);

	encrypt(data, size, encryption_key);
	dynamic_key.Encrypt(data, data, size, CRijndael::ECB);

	std::ostringstream data_stream;
	data_stream << encryption_key << std::endl << data;		//adds key used for xorstr / AES here
	data = const_cast<char*>(data_stream.str().c_str());

	data_stream.seekp(0, std::ios::end);					//gets new size with key added
	size = data_stream.tellp();
	data_stream.seekp(0, std::ios::beg);
	size -= data_stream.tellp();

	const_key.Encrypt(data, data, size, CRijndael::ECB);	//encrypt that key mf

	std::ofstream output;
	output.open("output.dll", std::ios::binary | std::ios::trunc);
	output.write(data, size);


	//now we try to decrypt output with same logic we would use on loader

	CRijndael const_dec_key, dynamic_dec_key;

	const_dec_key.MakeKey("i-pLAy-pOkeMOn-gO-eVErYdaY", CRijndael::sm_chain0, 32, 16); //this key is hardcoded

	std::ofstream dec;
	dec.open("decrypted.dll", std::ios::binary | std::ios::trunc);

	const_dec_key.Decrypt(data, data, size, CRijndael::ECB);	//decrypt first layer with hardcoded key

	std::istringstream encdata_stream(data);
	char *grabbed_key_str = new char[sizeof(long long)];
	encdata_stream.get(grabbed_key_str, std::numeric_limits<std::streamsize>::max());	//grab key
	std::istringstream grabbed_key_stream(grabbed_key_str);
	long long grabbed_key;

	if ((grabbed_key_stream >> grabbed_key).fail())	//can't convert grabbed key to long long
	{
		std::cout << "Decryption failure! can't grab key from file!" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		return -1;
	}

	long long grabbed_key_flip_shift = ~grabbed_key >> 69;

	std::ostringstream grabbed_key_ostream, grabbed_key_flip_shift_steam;
	grabbed_key_ostream << grabbed_key; grabbed_key_flip_shift_steam << grabbed_key_flip_shift;

	dynamic_dec_key.MakeKey(grabbed_key_ostream.str().c_str(), grabbed_key_flip_shift_steam.str().c_str(), 32, 16);	//now we get ready to decrypt 2nd layer
	char temp;					//but first we need to remove key from data
	encdata_stream.get(temp);	//The delimiting character is *not* extracted from the input sequence if found.

	int keyless_size = -encdata_stream.tellg();
	for (int i = 0; !encdata_stream.eof(); i++) //take away key from data basically
		encdata_stream.get(data[i]);

	keyless_size += encdata_stream.tellg();

	dynamic_dec_key.Decrypt(data, data, keyless_size, CRijndael::ECB); //finilly decrypt 2nd layer
	decrypt(data, keyless_size, grabbed_key);						   //and 3rd layer

	dec.write(data, size);

	return 0;
}