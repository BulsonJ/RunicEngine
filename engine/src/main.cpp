#include <Tracy.hpp>
#include <common/TracySystem.hpp>

#include "Engine.h"


int main(int argc, char* argv[])
{
	ZoneScoped;
	tracy::SetThreadName("MainThread");

	Engine game;
	game.init();
	game.run();
	game.deinit();

	return 0;
}