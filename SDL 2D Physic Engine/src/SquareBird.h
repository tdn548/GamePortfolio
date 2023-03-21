#pragma once
#ifndef __Square_Bird__
#define __Square_Bird__

#include "Sprite.h"

class SquareBird final : public Sprite
{

public:
	SquareBird(int w, int h);
	~SquareBird() override;

	// Life Cycle Methods
	virtual void Draw() override;
	virtual void Update() override;
	virtual void Clean() override;
};

#endif /* defined (__SquareBird__) */