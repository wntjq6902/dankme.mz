#define _WIN32_WINNT 0x0400


#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include "sha256.h"

int main()
{
	HW_PROFILE_INFO hwProfileInfo;
	std::string output;

	if (GetCurrentHwProfile(&hwProfileInfo) != NULL)
	{
		output = sha256(hwProfileInfo.szHwProfileGuid);
		printf("HWID code: %s\n", output.c_str());
		printf("send this value to ERRORNAME");
	}
	else
	{
		printf("Failed to get HWID!");
	}
	std::this_thread::sleep_for(std::chrono::seconds(30));
	return 0;
}