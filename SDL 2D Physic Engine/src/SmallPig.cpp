#include "SmallPig.h"
#include "SoundManager.h"
#include "TextureManager.h"

SmallPig::SmallPig(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/SmallPig.png", "SmallPig");

	const auto size = TextureManager::Instance().GetTextureSize("SmallPig");

	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	GetTransform()->position = glm::vec2(300.0f, 300.0f);

	SetType(GameObjectType::PIG);
	GetRigidBody()->isColliding = false;
}

SmallPig::~SmallPig()
= default;

void SmallPig::Draw()
{
	TextureManager::Instance().Draw("SmallPig", GetTransform()->position, 0, 255, true);
}

void SmallPig::Update()
{
}

void SmallPig::Clean()
{
}
