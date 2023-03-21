#pragma once
#ifndef __BLOCK__
#define __BLOCK__
#include "DisplayObject.h"

class Block final : public DisplayObject
{
public:
	// constructors
	Block(int w, int h);
	
	// destructor
	~Block() override;
	
	// life cycle functions
	void Draw() override;
	void Update() override;
	void Clean() override;
private:
	
};

#endif /* defined (__BLOCK__) */