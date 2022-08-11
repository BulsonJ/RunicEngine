#pragma once

#include <Runic/Engine.h>

#ifdef RNC_PLATFORM_WINDOWS

extern Runic::Application* Runic::CreateApplication();

int main(int argc, char* argv[])
{
	Engine eng;
	eng.init();

	auto app = Runic::CreateApplication();
	eng.run([=]() {app->Run(); });
	delete app;


	eng.deinit();
}

#endif