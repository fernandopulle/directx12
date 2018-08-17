#pragma once

#include <DirectXMath.h>
#include "DGraphics.h"


class Demo : public DGraphics {
public:
	Demo(UINT bufferCount, std::string name, LONG width, LONG height);
	void render();
};