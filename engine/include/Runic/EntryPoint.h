#pragma once

#ifdef RNC_PLATFORM_WINDOWS

extern Runic::Application* Runic::CreateApplication();

int main(int argc, char* argv[])
{
	auto app = Runic::CreateApplication();
	app->Run();
	delete app;
}

#endif