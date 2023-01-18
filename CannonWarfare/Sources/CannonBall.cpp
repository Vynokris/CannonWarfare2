#include "CannonBall.h"
#include "PhysicsConstants.h"
#include "ParticleManager.h"
#include "Arithmetic.h"
#include "RaylibConversions.h"
#include <sstream>
#include <iomanip>
using namespace Maths;


CannonBall::CannonBall(ParticleManager& _particleManager, const Maths::Vector2& startPosition, const Maths::Vector2& startVelocity, const float& predictedAirTime, const float& _groundHeight)
	: particleManager(_particleManager), groundHeight(_groundHeight)
{
	transform.rotateForwards = true;
	transform.position = startPosition;
	transform.velocity = startVelocity;
	transform.acceleration = { 0, GRAVITY };
	startPos = transform.position;
	startV   = transform.velocity;

	startTime = std::chrono::system_clock::now();

	const SpawnerParticleParams params = {
		ParticleShapes::POLYGON,
		transform.position,
		-PI, 0,
		5, 20,
		0, 0,
		0, 0,
		20, 35,
		0.05f, 0.2f,
		ORANGE,
	};
	particleManager.CreateSpawner(1, predictedAirTime, params, &transform);
}

Maths::Vector2 CannonBall::ComputeDrag() const
{
	const float dragCoeff = 0.5f * AIR_DENSITY * SPHERE_DRAG_COEFF * PI * sqpow(radius);
	return (transform.velocity * transform.velocity.GetLength()) * -dragCoeff;
}

void CannonBall::UpdateTrajectory()
{
	endTime = std::chrono::system_clock::now();
	airTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() / 1000.f;
	endPos  = transform.position;
	endV    = transform.velocity;
	controlPoint = LineIntersection(startPos, startV, endPos, -endV);
}

void CannonBall::Update(const float& deltaTime)
{
	// Update alphas depending on what is shown.
	if      ( showTrajectory && trajectoryAlpha < 1.f) trajectoryAlpha = clamp(trajectoryAlpha + deltaTime, 0, 1);
	else if (!showTrajectory && trajectoryAlpha > 0.f) trajectoryAlpha = clamp(trajectoryAlpha - deltaTime, 0, 1);
	
	// If the cannonball is above the ground, update its velocity and position.
	if (transform.position.y < groundHeight - radius * PIXEL_SCALE)
	{
		transform.Update(deltaTime);

		if (!landed) UpdateTrajectory();
	}

	// If the cannonball is under the ground...
	else if (transform.position.y > groundHeight - radius * PIXEL_SCALE)
	{
		// If it's the first ground it touches the ground, finalize the trajectory values.
		if (!landed)
		{
			UpdateTrajectory();
			landed = true;
		}

		// If it still has some velocity, make it bounce.
		if (transform.velocity.GetLength() > 10)
		{
			transform.position.y  = groundHeight - radius * PIXEL_SCALE - 0.01f;
			transform.velocity.y *= -1;
			transform.velocity.SetLength(transform.velocity.GetLength() * elasticity);
		}

		// If it has very little velocity, stop all its movement.
		else
		{
			transform.position.y   = groundHeight - radius * PIXEL_SCALE;
			transform.velocity     = {};
			transform.acceleration = {};
		}

		// Play landing particles.
		const SpawnerParticleParams params = {
			ParticleShapes::POLYGON,
			transform.position + Maths::Vector2(0, radius * PIXEL_SCALE * 1.5f),
			-PI/4, PI+PI/2,
			250, 500,
			0, 0,
			0, 0,
			20, 35,
			0.05f, 0.2f,
			WHITE,
		};
		particleManager.CreateSpawner(1, 0.1f, params);
	}

	// Update the destroy timer if it has started.
	if (0.f <= destroyTimer && destroyTimer <= destroyDuration)
	{
		destroyTimer -= deltaTime;
		color.a = (unsigned char)(255 * (destroyTimer / destroyDuration));
	}
}

void CannonBall::Draw() const
{
	// Draw the cannonball.
	DrawCircle     ((int)transform.position.x, (int)transform.position.y, radius * PIXEL_SCALE, BLACK);
	DrawCircleLines((int)transform.position.x, (int)transform.position.y, radius * PIXEL_SCALE, color);

	// Get the current trajectory color.
	const Color curColor = { color.r, color.g, color.b, (unsigned char)min(trajectoryAlpha * 255, color.a) };
	
	// Draw air time.
	std::stringstream textValue; textValue << std::fixed << std::setprecision(2) << airTime << "s";
	const Maths::Vector2 textPos = { transform.position.x - MeasureText(textValue.str().c_str(), 20) / 2.f, transform.position.y - 10 };
	DrawText(textValue.str().c_str(), (int)textPos.x, (int)textPos.y, 20, curColor);
}

void CannonBall::DrawTrajectory() const
{
	// Draw the trajectory with a bezier curve.
	const Color curColor = { color.r, color.g, color.b, (unsigned char)min(trajectoryAlpha * 255, color.a) };
	DrawLineBezierQuad(ToRayVector2(startPos), ToRayVector2(endPos), ToRayVector2(controlPoint), 1, curColor);
	DrawPoly(ToRayVector2(endPos), 3, 12, radToDeg(endV.GetAngle()) - 90, curColor);
}

void CannonBall::Destroy()
{
	// Start the destroy timer.
	destroyTimer = destroyDuration;
}
