#include "LongBlock.h"


#include "SoundManager.h"
#include "TextureManager.h"

LongBlock::LongBlock(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/LongBlock.png", "LongBlock");

	const auto size = TextureManager::Instance().GetTextureSize("LongBlock");
	
	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	GetTransform()->position = glm::vec2(300.0f, 300.0f);

	SetType(GameObjectType::OBSTACLE);
	GetRigidBody()->isColliding = false;


}

LongBlock::~LongBlock()
= default;

void LongBlock::Draw()
{
	TextureManager::Instance().Draw("LongBlock", GetTransform()->position, 0, 255, true);
}

void LongBlock::Update()
{
}

void LongBlock::Clean()
{
}
