#include "SquareBird.h"
#include "TextureManager.h"

SquareBird::SquareBird(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/SquareBird.png", "SquareBird");

	const auto size = TextureManager::Instance().GetTextureSize("SquareBird");


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

SquareBird::~SquareBird()
= default;

void SquareBird::Draw()
{
	TextureManager::Instance().Draw("SquareBird", GetTransform()->position, 0, 255, true);
}

void SquareBird::Update()
{
}

void SquareBird::Clean()
{
}