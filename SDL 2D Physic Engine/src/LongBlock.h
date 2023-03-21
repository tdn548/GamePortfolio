#pragma once
#ifndef __LONGBLOCK__
#define __LONGBLOCK__
#include "DisplayObject.h"

class LongBlock final : public DisplayObject
{
public:
	// constructors
	LongBlock(int w, int h);
	
	// destructor
	~LongBlock() override;
	
	// life cycle functions
	void Draw() override;
	void Update() override;
	void Clean() override;
private:
	
};

#endif /* defined (__LONGBLOCK__) */