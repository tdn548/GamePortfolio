#pragma once
#ifndef __GROUND__
#define __GROUND__
#include "DisplayObject.h"

class Ground final : public DisplayObject
{
public:
	// constructors
	Ground(int w, int h);
	
	// destructor
	~Ground() override;
	
	// life cycle functions
	void Draw() override;
	void Update() override;
	void Clean() override;
private:
	
};

#endif /* defined (__GROUND__) */