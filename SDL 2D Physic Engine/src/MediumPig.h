#pragma once
#ifndef __MEDIUMPIG__
#define __MEDIUMPIG__
#include "DisplayObject.h"

class MediumPig final : public DisplayObject
{
public:
	// constructors
	MediumPig(int w, int h);
	
	// destructor
	~MediumPig() override;
	
	// life cycle functions
	void Draw() override;
	void Update() override;
	void Clean() override;
	int GetPoints() { return points; }
private:
	int points = 2;
};

#endif /* defined (__MEDIUMPIG__) */