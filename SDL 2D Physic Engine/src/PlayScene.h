#pragma once
#ifndef __PLAY_SCENE__
#define __PLAY_SCENE__

#include <iostream>
#include "Scene.h"
#include "Plane.h"
#include "Bird.h"
#include "SquareBird.h"
#include "Button.h"
#include "Label.h"
#include "PhysicsEngine.h"
#include "HalfPlane.h"
#include "SmallPig.h"
#include "MediumPig.h"
#include "BigPig.h"
#include "Block.h"
#include "BigBlock.h"
#include "LongBlock.h"
#include "Ground.h"

const float DELTA_TIME = 1.0 / 60.0f;

class PlayScene : public Scene
{
public:
	PlayScene();
	~PlayScene() override;

	// Scene LifeCycle Functions
	virtual void Draw() override;
	virtual void Update() override;
	virtual void Clean() override;
	virtual void HandleEvents() override;
	virtual void Start() override;
private:
	// IMGUI Function
	void GUI_Function();

	std::string m_guiTitle;

	GameObject* m_pProjectile{};

	Bird* m_pBird{};
	SquareBird* m_pSquareBird{};
	SmallPig* m_pSmallPig{};
	MediumPig* m_pMediumPig{};
	BigPig* m_pBigPig{};

	Block* m_pBlock{};
	Block* m_pBlock2{};
	Block* m_pBlock3{};
	Block* m_pBlock4{};
	Block* m_pBlock5{};
	Block* m_pBlock6{};
	BigBlock* m_pBigBlock{};
	LongBlock* m_pLongBlock{};
	Ground* m_pGround{};

	// UI Items
	Button* m_pBackButton{};
	Button* m_pNextButton{};
	Label* m_pInstructionsLabel{};

	Label* m_pScoreLabel = nullptr;
	Label* m_pInstructionLabel = nullptr;
	Label* m_pInstructionLabel2 = nullptr;
	int score;
	void GetKeyboardInput();

	float startingY = 250;
	float lunchAngle = 45;
	float lunchSpeed = 1200;
	float accelerationGravity = -918.0;
	float friction = 0.96;

	// It will be multiplied by the distance between the bird and slignshot
	float slingShotPower = 20000;

	bool b_shootBullet = false;
	bool m_playerSelected = false;

	PhysicsEngine* physicsEngine;

	float m_angleHalfPlane = 90;
	float m_angleHalfPlane2 = 156.5;

	glm::vec2 starting_point = glm::vec2(180, 400);
	glm::vec2 idle_point = glm::vec2(50, 474);
	glm::vec2 BirdPosPreviousFrame;


	bool m_pSmallRemoved = false;
	bool m_pMediumRemoved = false;
	bool m_pBigRemoved = false;

	const float NORMAL_RENDER_SCALE = 100;
	glm::vec2 normal = { 0.0f, -1.0f };
	bool m_pDrawHalfplane = false;
	bool m_pItIsDrew = false;
};

#endif /* defined (__PLAY_SCENE__) */