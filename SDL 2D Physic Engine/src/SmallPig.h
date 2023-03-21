#pragma once
#ifndef __SMALLPIG__
#define __SMALLPIG__
#include "DisplayObject.h"

class SmallPig final : public DisplayObject
{
public:
	// constructors
	SmallPig(int w, int h);
	
	// destructor
	~SmallPig() override;
	
	// life cycle functions
	void Draw() override;
	void Update() override;
	void Clean() override;
	int GetPoints() { return points; }
private:
	int points = 1;
	
};

#endif /* defined (__SMALLPIG__) */