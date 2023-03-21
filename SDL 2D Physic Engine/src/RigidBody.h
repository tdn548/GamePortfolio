#pragma once
#ifndef __RIGID_BODY__
#define __RIGID_BODY__
#include <glm/vec2.hpp>

class GameObject;
class Label;

struct RigidBody
{
	int score = 0;
	GameObject* gameObject = nullptr;
	float mass = 100.0f;
	glm::vec2 velocity = glm::vec2(0,0);
	glm::vec2 acceleration = glm::vec2(0, 0);
	bool isColliding = false;

	// Collision Detection
	float radius = 15.0f;
	double time = 0;

	// Lab 9
	glm::vec2 fFriction;
	glm::vec2 fNormal;
	glm::vec2 fPerpendicularGravity;
	glm::vec2 fParallelGravity;

	glm::vec2 netForce;
	glm::vec2 gravityScale = { 0,-1 };
	float friction = 0.1;
	float restitution = 0.9;
	float toughness = 20000;

	bool enableGravity = true;
	bool wasKilled = false;
	bool isActive = false;
};

#endif /* defined (__RIGID_BODY__) */