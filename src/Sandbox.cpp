#include <Runic.h>

class Sandbox : public Runic::Application
{
public:
	Sandbox() 
	{
		Engine eng;
		eng.init();
		eng.run();
		eng.deinit();
	}

	~Sandbox()
	{

	}
};

Runic::Application* Runic::CreateApplication() {
	return new Sandbox();
}