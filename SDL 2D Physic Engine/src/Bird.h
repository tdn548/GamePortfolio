#pragma once
#ifndef __BIRD__
#define __BIRD__

#include "Sprite.h"

class Bird final : public Sprite
{

public:
	Bird(int w, int h);
	~Bird() override;

	// Life Cycle Methods
	virtual void Draw() override;
	virtual void Update() override;
	virtual void Clean() override;
};

#endif /* defined (__BIRD__) */