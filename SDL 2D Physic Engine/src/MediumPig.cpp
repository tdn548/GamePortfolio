#include "MediumPig.h"
#include "SoundManager.h"
#include "TextureManager.h"

MediumPig::MediumPig(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/MediumPig.png", "MediumPig");

	const auto size = TextureManager::Instance().GetTextureSize("MediumPig");

	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	GetTransform()->position = glm::vec2(300.0f, 300.0f);

	SetType(GameObjectType::PIG);
	GetRigidBody()->isColliding = false;
}

MediumPig::~MediumPig()
= default;

void MediumPig::Draw()
{
	TextureManager::Instance().Draw("MediumPig", GetTransform()->position, 0, 255, true);
}

void MediumPig::Update()
{
}

void MediumPig::Clean()
{
}
