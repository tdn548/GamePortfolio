#pragma once
#ifndef __HALF_PLANE__
#define __HALF_PLANE__

#include "DisplayObject.h"

class HalfPlane : public DisplayObject
{
public:
	void Draw() override;
	void Update() override;
	void Clean() override;

	void SetOrientation(float orientation);

	glm::vec2 GetNormal();
	float GetOrientation();
private:
	const float NORMAL_RENDER_SCALE = 100;
	glm::vec2 normal = { 0.0f, -1.0f };
	float m_orientationAngle = 0;
};



#endif /* defined (__HALF_PLANE__) */