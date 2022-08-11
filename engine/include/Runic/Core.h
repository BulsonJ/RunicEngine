#pragma once

#ifdef RNC_PLATFORM_WINDOWS
	#ifdef RNC_BUILD_DLL
		#define RUNIC_API __declspec(dllexport)
	#else
		#define RUNIC_API __declspec(dllimport)
	#endif
#endif