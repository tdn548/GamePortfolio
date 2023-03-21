#include "PhysicsEngine.h"
#include "GameObject.h"
#include "Bird.h"
#include "Util.h"
#include "PlayScene.h"

double calcTime(clock_t clock1, clock_t clock2)
{
	double ticks = clock1 - clock2;	// Difference between 2 separate clock ticks.
	double ticksToMS = (ticks * 10) / CLOCKS_PER_SEC;
	double msToSec = ticksToMS / 10;
	return msToSec;
}

void PhysicsEngine::UpdatePhysics()
{
	for (auto it = physicsObjects.begin(); it != physicsObjects.end(); it++)
	{
		if (onSlingshot == false)
		{
			// Reference iterator to get element
			RigidBody* rb = (*it);

			if (rb->enableGravity)
			{
				// Apply Friction to velocity
				rb->velocity *= airFriction;

				// Apply gravity force
				glm::vec2 fGravity = gravity * rb->gravityScale * rb->mass;

				rb->netForce += fGravity;
				glm::vec2 acceleration = rb->netForce / rb->mass;

				// Apply gravity acceleretation to velocity
				rb->velocity += acceleration * fixedDeltaTime;

				// Apply velocity to the position
				rb->gameObject->GetTransform()->position += rb->velocity * fixedDeltaTime;
				rb->netForce = glm::vec2(0, 0);
			}
		}
	}
}

void PhysicsEngine::ObjectHalfPlaneCollision(HalfPlane* halfplane)
{
	for (auto it = physicsObjects.begin(); it != physicsObjects.end(); it++)
	{
		RigidBody* rb = (*it);

		glm::vec2 vectorObject = { rb->gameObject->GetTransform()->position.x - halfplane->GetTransform()->position.x,
			rb->gameObject->GetTransform()->position.y - halfplane->GetTransform()->position.y };

		float dotProduct = (vectorObject.x * halfplane->GetNormal().x) + (vectorObject.y * halfplane->GetNormal().y);

		glm::vec2 vectorProjection = { (dotProduct - rb->radius) * (halfplane->GetNormal().x), 
			(dotProduct - rb->radius) * (halfplane->GetNormal().y) };


		if (dotProduct < rb->radius)
		{
			rb->gameObject->GetTransform()->position += -vectorProjection;

			/********* Lab 9 *******/

			glm::vec2 fGravity = gravity * rb->gravityScale * rb->mass;
			float gravityDotNormal = Util::Dot(fGravity, halfplane->GetNormal());
			glm::vec2 fGravityPerpendicular = gravityDotNormal * halfplane->GetNormal();
			glm::vec2 fGravityParalllel = fGravity - fGravityPerpendicular;

			rb->fParallelGravity = fGravityParalllel;
			rb->fPerpendicularGravity = fGravityPerpendicular;
			rb->fNormal = -fGravityPerpendicular;
			float fNormalMagnitud = abs(gravityDotNormal);

			glm::vec2 frictionDirection = -Util::Normalize(fGravityParalllel);

			// Use a coefficient
			float k_friction = rb->friction;

			// Ensure friction is never greater than parallel gravity component
			float frictionMagnitud = Util::Min(k_friction * fNormalMagnitud, Util::Magnitude(fGravityParalllel));

			// Friction force vector is dependent on the direction of gravity parallele to the surface 
			rb->fFriction = k_friction * frictionMagnitud * (frictionDirection);

			// Apply Friction
			rb->netForce += rb->fFriction;

			// Apply Normal
			rb->netForce += rb->fNormal;

			/*************************/

			rb->isColliding = true;
		}
		else
		{
			rb->fParallelGravity = { 0,0 };
			rb->fPerpendicularGravity = { 0,0 };
			rb->fNormal = { 0,0 };
			rb->fFriction = { 0,0 };

			
		}
	}
}

void PhysicsEngine::SetGravity(float g)
{
	gravity = g;
}

void PhysicsEngine::SetFriction(float f)
{
	airFriction = f;
}

void PhysicsEngine::SetOnSlingshot(bool on)
{
	onSlingshot = on;
}

bool PhysicsEngine::GetOnSlingshot()
{
	return onSlingshot;
}

void PhysicsEngine::AddCircleObject(RigidBody* circle)
{
	physicsObjects.push_back(circle);
	circles.push_back(circle);
}

void PhysicsEngine::AddRectangleObject(RigidBody* rectangle)
{
	physicsObjects.push_back(rectangle);
	rectangles.push_back(rectangle);
}


void PhysicsEngine::RemoveCircleObject(RigidBody* object)
{
	physicsObjects.remove(object);
	circles.remove(object);
}

void PhysicsEngine::RemoveObject(RigidBody* object)
{
	physicsObjects.remove(object);
}

void PhysicsEngine::CircleCircleCollision()
{
	for (auto it = circles.begin(); it != circles.end(); it++)
	{
		auto it2 = it;
		it2++;
		for (; it2 != circles.end(); it2++)
		{
			// Deference iterator to get the element
			RigidBody* rb = (*it);
			RigidBody* rb2 = (*it2);

			//Find the distance between 2 object
			float m_distance = sqrt(pow(abs(rb->gameObject->GetTransform()->position.x - rb2->gameObject->GetTransform()->position.x), 2) +
				pow(abs(rb->gameObject->GetTransform()->position.y - rb2->gameObject->GetTransform()->position.y), 2));

			//if distanceBetween < sum of radius then they are overlapping. Print name of who collided?
			if (m_distance <= (rb->radius + rb2->radius))
			{

				glm::vec2 displacementBRelativeA = rb2->gameObject->GetTransform()->position - rb->gameObject->GetTransform()->position;
				float distanceAB = Util::Magnitude(displacementBRelativeA);

				// check distance between agains radius

				float overlap = distanceAB - (rb->radius + rb2->radius);

				if (overlap > 0)
				{
					continue;
				}

				glm::vec2  collisionNormalAtoB = displacementBRelativeA / distanceAB;
				// get ralative velocity projected along normal
				glm::vec2 velocityBRelativeA = rb2->velocity - rb->velocity;

				//ARE THEY MOVING TOWARD OR AWAY FROM EACH OTHER
				float closingRate = Util::Dot(velocityBRelativeA, collisionNormalAtoB);

				//Separate the circles by minium translation vector
				glm::vec2 minimumTranslationVector = collisionNormalAtoB * overlap;

				//Bounce
				float restitution = Util::Min(rb->restitution, rb2->restitution);

				float totalMass = rb->mass + rb2->mass;
				float impulse = -(1.0f + restitution) * closingRate * rb->mass * rb2->mass / (totalMass);

				glm::vec2  impulseA = -impulse * collisionNormalAtoB;
				glm::vec2  impulseB = impulse * collisionNormalAtoB;

				//to apply impulse, just divide by mass 
				glm::vec2 deltaVA = impulseA / rb->mass;
				glm::vec2 deltaVB = impulseB / rb2->mass;

				if (deltaVA.x > deltaVB.x || deltaVA.y > deltaVB.y)
				{
					rb->gameObject->GetTransform()->position += minimumTranslationVector;
				
				}
				else if (deltaVB.x > deltaVA.x || deltaVB.y > deltaVA.y)
				{
					rb2->gameObject->GetTransform()->position -= minimumTranslationVector;
				}

				if (closingRate < 0)
				{
					//apply changes in velocity to objects
					rb->velocity += deltaVA;
					rb2->velocity += deltaVB;

					if (rb->gameObject->GetType() == GameObjectType::PIG && impulse >= rb->toughness)
					{
						rb->wasKilled = true;
					}
					else if (rb2->gameObject->GetType() == GameObjectType::PIG && impulse >= rb2->toughness)
					{
						rb2->wasKilled = true;
					}
				}
			}
		}
	}
}

void PhysicsEngine::AABBAABBCollision()
{
	for (auto it = rectangles.begin(); it != rectangles.end(); it++)
	{
		auto it2 = it;
		it2++;
		for (; it2 != rectangles.end(); it2++)
		{
			// Deference iterator to get the element
			RigidBody* rb = (*it);
			RigidBody* rb2 = (*it2);

			float minimumTransX = MinimumTranslationVector1D(rb->gameObject->GetTransform()->position.x, rb->gameObject->GetWidth() / 2,
				rb2->gameObject->GetTransform()->position.x, rb2->gameObject->GetWidth() / 2);

			float minimumTransY = MinimumTranslationVector1D(rb->gameObject->GetTransform()->position.y, rb->gameObject->GetHeight() / 2,
				rb2->gameObject->GetTransform()->position.y, rb2->gameObject->GetHeight() / 2);

			if (minimumTransX == 0 && minimumTransY == 0) //
			{
				continue;
			}
			else if (minimumTransX != 0 && minimumTransY != 0)
			{
				glm::vec2 displacementBRelativeA = rb2->gameObject->GetTransform()->position - rb->gameObject->GetTransform()->position;
				float distanceAB = Util::Magnitude(displacementBRelativeA);

				glm::vec2  collisionNormalAtoB = displacementBRelativeA / distanceAB;

				// get ralative velocity projected along normal
				glm::vec2 velocityBRelativeA = rb2->velocity - rb->velocity;

				//ARE THEY MOVING TOWARD OR AWAY FROM EACH OTHER
				float closingRate = Util::Dot(velocityBRelativeA, collisionNormalAtoB);


				//Bounce
				float restitution = Util::Min(rb2->restitution, rb->restitution);

				float totalMass = rb2->mass + rb->mass;
				float impulse = -(1.0f + restitution) * closingRate * rb->mass * rb2->mass / (totalMass);

				glm::vec2  impulseA = -impulse * collisionNormalAtoB;
				glm::vec2  impulseB = impulse * collisionNormalAtoB;

				//to apply impulse, just divide by mass 
				glm::vec2 deltaVA = impulseA / rb->mass;
				glm::vec2 deltaVB = impulseB / rb2->mass;


				glm::vec2 mtv2D;

				if (abs(minimumTransX) < abs(minimumTransY)) // if the amount we would need to move them by the smaller in the x direction
				{
					// move along x because it's less effort (minimum translation)
					mtv2D = glm::vec2(minimumTransX, 0);

					if (deltaVA.x > deltaVB.x || deltaVA.y > deltaVB.y)
					{
						if (rb->gameObject->GetType() != GameObjectType::OBSTACLE)
						{
							rb->gameObject->GetTransform()->position += mtv2D.y;
						}
						else
						{
							rb2->gameObject->GetTransform()->position += mtv2D.y;

						}

					}
					else if (deltaVB.x > deltaVA.x || deltaVB.y > deltaVA.y)
					{

						if (rb2->gameObject->GetType() != GameObjectType::OBSTACLE)
						{
							rb2->gameObject->GetTransform()->position += mtv2D.x;
						}
						else
						{
							rb->gameObject->GetTransform()->position += mtv2D.x;
						}
					}
				}
				else
				{
					// move along y
					mtv2D = glm::vec2(0, minimumTransY);

					if (rb->gameObject->GetTransform()->position.y < rb2->gameObject->GetTransform()->position.y)
					{
						rb->gameObject->GetTransform()->position.y += mtv2D.y;
					}
					else
					{
						rb2->gameObject->GetTransform()->position.y -= mtv2D.y;
					}
				}

				if (closingRate < 0)
				{
					//apply changes in velocity to objects
					rb->velocity += deltaVA;
					rb2->velocity += deltaVB;
				}
			}
		}
	}
}

float PhysicsEngine::MinimumTranslationVector1D(const float centerA, const float radiusA, const float centerB, const float radiusB)
{
	// Vector between the objects from centerA to centerB
	float displacementAtoB = centerB - centerA;

	// Find distance
	float distance = abs(displacementAtoB);

	// Detect overlapping
	float radiiSum = radiusA + radiusB;
	float overlap = distance - radiiSum;

	if (overlap > 0)
	{
		return 0.0f;
	}

	// Are overlapping
	float directionAtoB = Util::Sign(displacementAtoB);
	float minimumTranslationVector = directionAtoB * overlap;

	return minimumTranslationVector;
}

void PhysicsEngine::CircleAABBCollision()
{
	for (auto it = rectangles.begin(); it != rectangles.end(); it++)
	{
		for (auto it2 = circles.begin(); it2 != circles.end(); it2++)
		{
			// Deference iterator to get the element
			RigidBody* rect = (*it);
			RigidBody* circle = (*it2);

			float clampX = Util::Clamp(circle->gameObject->GetTransform()->position.x,
				rect->gameObject->GetTransform()->position.x - rect->gameObject->GetWidth() / 2,
				rect->gameObject->GetTransform()->position.x + rect->gameObject->GetWidth() / 2);

			float clampY = Util::Clamp(circle->gameObject->GetTransform()->position.y,
				rect->gameObject->GetTransform()->position.y - rect->gameObject->GetHeight() / 2,
				rect->gameObject->GetTransform()->position.y + rect->gameObject->GetHeight() / 2);

			glm::vec2 clampPoint = glm::vec2(clampX, clampY);

			float distance = abs(Util::Magnitude(circle->gameObject->GetTransform()->position - clampPoint));

			if (distance > circle->radius)
			{
				continue;
			}

			glm::vec2 normalRelativePosRectToCircle = Util::Normalize(clampPoint - circle->gameObject->GetTransform()->position);
			glm::vec2 mtv = normalRelativePosRectToCircle * (circle->radius - distance);

			glm::vec2 displacementBRelativeA = rect->gameObject->GetTransform()->position - circle->gameObject->GetTransform()->position;
			float distanceAB = Util::Magnitude(displacementBRelativeA);

			glm::vec2  collisionNormalAtoB = displacementBRelativeA / distanceAB;

			// get ralative velocity projected along normal
			glm::vec2 velocityBRelativeA = rect->velocity - circle->velocity;

			//ARE THEY MOVING TOWARD OR AWAY FROM EACH OTHER
			float closingRate = Util::Dot(velocityBRelativeA, collisionNormalAtoB);

			//Bounce
			float restitution = Util::Min(circle->restitution, rect->restitution);

			float totalMass = rect->mass + circle->mass;
			float impulse = -(1.0f + restitution) * closingRate * circle->mass * rect->mass / (totalMass);

			glm::vec2  impulseA = -impulse * collisionNormalAtoB;
			glm::vec2  impulseB = impulse * collisionNormalAtoB;

			//to apply impulse, just divide by mass 
			glm::vec2 deltaVA = impulseA / circle->mass;
			glm::vec2 deltaVB = impulseB / rect->mass;


			if (rect->gameObject->GetType() != GameObjectType::OBSTACLE)
			{
				rect->gameObject->GetTransform()->position += mtv;
			}
			else
			{
				if (rect->gameObject->GetTransform()->position.y < circle->gameObject->GetTransform()->position.y)
				{
					rect->gameObject->GetTransform()->position += mtv;
				}
				else
				{
					circle->gameObject->GetTransform()->position -= mtv;
				}
			}

			if (closingRate < 0)
			{
				//apply changes in velocity to objects
				circle->velocity += deltaVA;
				rect->velocity += deltaVB;

				if (circle->gameObject->GetType() == GameObjectType::PIG && impulse >= circle->toughness * 10)
				{
					circle->wasKilled = true;
				}
				else if (circle->gameObject->GetType() == GameObjectType::PIG && rect->gameObject->GetType() == GameObjectType::PLAYER && impulse >= circle->toughness)
				{
					circle->wasKilled = true;
				}
			}
		}
	}
}

clock_t PhysicsEngine::endTime = 0;
clock_t PhysicsEngine::startTime = 0;