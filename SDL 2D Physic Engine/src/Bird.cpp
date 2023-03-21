#include "Bird.h"
#include "TextureManager.h"

Bird::Bird(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/Bird.png", "Bird");

	const auto size = TextureManager::Instance().GetTextureSize("Bird");


	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	GetTransform()->position = glm::vec2(400.0f, 300.0f);
	GetRigidBody()->velocity = glm::vec2(0.0f, 0.0f);
	GetRigidBody()->acceleration = glm::vec2(0.0f, 0.0f);
	GetRigidBody()->isColliding = false;
	SetType(GameObjectType::PLAYER);
}

Bird::~Bird()
= default;

void Bird::Draw()
{
	TextureManager::Instance().Draw("Bird", GetTransform()->position, 0, 255, true);
}

void Bird::Update()
{
}

void Bird::Clean()
{
}
