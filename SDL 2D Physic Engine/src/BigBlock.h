#pragma once
#ifndef __BIGBLOCK__
#define __BIGBLOCK__
#include "DisplayObject.h"

class BigBlock final : public DisplayObject
{
public:
	// constructors
	BigBlock(int w, int h);
	
	// destructor
	~BigBlock() override;
	
	// life cycle functions
	void Draw() override;
	void Update() override;
	void Clean() override;
private:
	
};

#endif /* defined (__BIGBLOCK__) */