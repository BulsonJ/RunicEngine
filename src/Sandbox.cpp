#include <Runic.h>

class Sandbox : public Runic::Application
{
public:
	Sandbox() 
	{

	}

	~Sandbox()
	{

	}

	virtual void Run() override
	{
		
	}
};

Runic::Application* Runic::CreateApplication() {
	return new Sandbox();
}