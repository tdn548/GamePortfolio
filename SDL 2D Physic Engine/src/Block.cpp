#include "Block.h"


#include "SoundManager.h"
#include "TextureManager.h"

Block::Block(int w, int h)
{
	TextureManager::Instance().Load("../Assets/textures/Block.png", "Block");

	const auto size = TextureManager::Instance().GetTextureSize("Block");

	GetRigidBody()->radius = w / 2;

	// set frame width
	SetWidth(w);

	// set frame height
	SetHeight(h);

	GetTransform()->position = glm::vec2(300.0f, 300.0f);

	SetType(GameObjectType::OBSTACLE);
	GetRigidBody()->isColliding = false;


}

Block::~Block()
= default;

void Block::Draw()
{
	TextureManager::Instance().Draw("Block", GetTransform()->position, 0, 255, true);
}

void Block::Update()
{
}

void Block::Clean()
{
}
