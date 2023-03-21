#include "HalfPlane.h"
#include "Util.h"


void HalfPlane::SetOrientation(float orientation)
{
	m_orientationAngle = orientation;
	normal = Util::AngleMagnitudToVector(orientation, 1.0f);
}

float HalfPlane::GetOrientation()
{
	return m_orientationAngle;
}

glm::vec2 HalfPlane::GetNormal()
{
	return normal;
}

void HalfPlane::Draw()
{

}

void HalfPlane::Update()
{
}

void HalfPlane::Clean()
{
}