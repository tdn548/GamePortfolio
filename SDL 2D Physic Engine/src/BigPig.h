#pragma once
#ifndef __BIGPIG__
#define __BIGPIG__
#include "DisplayObject.h"

class BigPig final : public DisplayObject
{
public:
	// constructors
	BigPig(int w, int h);
	
	// destructor
	~BigPig() override;
	
	// life cycle functions
	void Draw() override;
	void Update() override;
	void Clean() override;
	
	int GetPoints() { return points;  }
private:
	int points = 3;
};

#endif /* defined (__BIGPIG__) */