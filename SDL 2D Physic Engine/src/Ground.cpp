#include "Ground.h"
#include "SoundManager.h"
#include "TextureManager.h"

Ground::Ground(int w, int h)
{
	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	GetTransform()->position = glm::vec2(300.0f, 300.0f);

	SetType(GameObjectType::OBSTACLE);
	GetRigidBody()->isColliding = false;

}

Ground::~Ground()
= default;

void Ground::Draw()
{
	
}

void Ground::Update()
{
}

void Ground::Clean()
{
}
