#include "BigBlock.h"


#include "SoundManager.h"
#include "TextureManager.h"

BigBlock::BigBlock(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/BigBlock.png", "BigBlock");

	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	const auto size = TextureManager::Instance().GetTextureSize("BigBlock");
	SetWidth(static_cast<int>(size.x));
	SetHeight(static_cast<int>(size.y));

	GetTransform()->position = glm::vec2(300.0f, 300.0f);

	SetType(GameObjectType::OBSTACLE);
	GetRigidBody()->isColliding = false;


}

BigBlock::~BigBlock()
= default;

void BigBlock::Draw()
{
	TextureManager::Instance().Draw("BigBlock", GetTransform()->position, 0, 255, true);
}

void BigBlock::Update()
{
}

void BigBlock::Clean()
{
}
