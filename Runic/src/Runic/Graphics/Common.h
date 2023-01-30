#pragma once

namespace GFX {
	enum Stages
	{
		VERTEX = 1,
		FRAGMENT = 2,
		ALL = FRAGMENT | VERTEX
	};

	namespace Buffer
	{
		enum class Usage
		{
			NONE,
			UNIFORM,
			STORAGE,
			VERTEX,
			INDEX,
		};
	}
}
