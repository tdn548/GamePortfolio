#include "BigPig.h"
#include "SoundManager.h"
#include "TextureManager.h"

BigPig::BigPig(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/LargePig.png", "BigPig");

	const auto size = TextureManager::Instance().GetTextureSize("BigPig");

	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	GetTransform()->position = glm::vec2(300.0f, 300.0f);

	SetType(GameObjectType::PIG);
	GetRigidBody()->isColliding = false;
}

BigPig::~BigPig()
= default;

void BigPig::Draw()
{
	TextureManager::Instance().Draw("BigPig", GetTransform()->position, 0, 255, true);
}

void BigPig::Update()
{
}

void BigPig::Clean()
{
}
