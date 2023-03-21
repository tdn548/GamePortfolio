#pragma once
#ifndef __PHYSICS_ENGINE__
#define __PHYSICS_ENGINE__
#include "SDL.h"
#include <vector>
#include <iostream>
#include <list>
#include <sstream>
#include "RigidBody.h"
#include "HalfPlane.h"
#include "Label.h"
#include "time.h"
class GameObject;

class PhysicsEngine
{
public:
	void UpdatePhysics();
	void ObjectHalfPlaneCollision(HalfPlane* halfplane);

	void SetGravity(float g);
	void SetFriction(float f);
	void SetOnSlingshot(bool on);
	bool GetOnSlingshot();

	void AddCircleObject(RigidBody* circle);
	void AddRectangleObject(RigidBody* rectangle);

	void RemoveCircleObject(RigidBody* object);
	void RemoveObject(RigidBody* object);
	void CircleCircleCollision();
	void AABBAABBCollision();
	void CircleAABBCollision();
	
private:
	float MinimumTranslationVector1D(const float centerA, const float radiusA, const float centerB, const float radiusB);

	std::list<RigidBody*>physicsObjects;
	std::list<RigidBody*>rectangles;
	std::list<RigidBody*>circles;

	float gravity;
	float airFriction;
	const float fixedDeltaTime = 0.016f;
	bool onSlingshot = true;
	bool Score = false;
	static clock_t endTime;
	static clock_t startTime;

};


#endif /* defined (__PHYSICS_ENGINE__) */
