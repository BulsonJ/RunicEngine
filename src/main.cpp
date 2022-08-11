#include "Engine.h"

int main(int argc, char* argv[])
{
	Engine game;
	game.init();
	game.run();
	game.deinit();

	return 0;
}