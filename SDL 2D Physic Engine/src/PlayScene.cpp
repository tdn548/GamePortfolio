#include "PlayScene.h"
#include "Game.h"
#include "EventManager.h"
#include "InputType.h"

// required for IMGUI
#include "imgui.h"
#include "imgui_sdl.h"
#include "Renderer.h"
#include "Util.h"

PlayScene::PlayScene()
{
	PlayScene::Start();
}

PlayScene::~PlayScene()
= default;

void PlayScene::Draw()
{
	TextureManager::Instance().Draw("background", 0,0);

	if (physicsEngine->GetOnSlingshot() == true)
	{
		Util::DrawLine({ starting_point.x + 10, starting_point.y }, m_pProjectile->GetTransform()->position, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x + 10, starting_point.y + 1 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y + 1 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x + 10, starting_point.y + 2 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y + 2 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x + 10, starting_point.y + 3 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y + 3 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x + 10, starting_point.y + 4 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y + 4 }, { 0.89,0.65,0,34 });

		Util::DrawLine({ starting_point.x + 10, starting_point.y - 1 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y - 1 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x + 10, starting_point.y - 2 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y - 2 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x + 10, starting_point.y - 3 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y - 3 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x + 10, starting_point.y - 4 }, { m_pProjectile->GetTransform()->position.x, m_pProjectile->GetTransform()->position.y - 4 }, { 0.89,0.65,0,34 });
	}

	DrawDisplayList();
	SDL_SetRenderDrawColor(Renderer::Instance().GetRenderer(), 255, 255, 255, 255);

	if (physicsEngine->GetOnSlingshot() == true)
	{
		Util::DrawLine({ starting_point.x, starting_point.y + 10 }, { m_pProjectile->GetTransform()->position.x - 20, m_pProjectile->GetTransform()->position.y + 10 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x, starting_point.y + 11 }, { m_pProjectile->GetTransform()->position.x - 19, m_pProjectile->GetTransform()->position.y + 11 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x, starting_point.y + 12 }, { m_pProjectile->GetTransform()->position.x - 18, m_pProjectile->GetTransform()->position.y + 12 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x, starting_point.y + 13 }, { m_pProjectile->GetTransform()->position.x - 17, m_pProjectile->GetTransform()->position.y + 13 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x, starting_point.y + 14 }, { m_pProjectile->GetTransform()->position.x - 16, m_pProjectile->GetTransform()->position.y + 14 }, { 0.89,0.65,0,34 });

		Util::DrawLine({ starting_point.x, starting_point.y + 15 }, { m_pProjectile->GetTransform()->position.x - 15, m_pProjectile->GetTransform()->position.y + 15 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x, starting_point.y + 16 }, { m_pProjectile->GetTransform()->position.x - 14, m_pProjectile->GetTransform()->position.y + 16 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x, starting_point.y + 17 }, { m_pProjectile->GetTransform()->position.x - 13, m_pProjectile->GetTransform()->position.y + 17 }, { 0.89,0.65,0,34 });
		Util::DrawLine({ starting_point.x, starting_point.y + 18 }, { m_pProjectile->GetTransform()->position.x - 12, m_pProjectile->GetTransform()->position.y + 18 }, { 0.89,0.65,0,34 });
	}

	Util::DrawRect(m_pGround->GetTransform()->position, m_pGround->GetWidth(), m_pGround->GetHeight());
}


void PlayScene::Update()
{
	UpdateDisplayList();


	physicsEngine->SetGravity(accelerationGravity);
	physicsEngine->SetFriction(friction);
	physicsEngine->UpdatePhysics();
	physicsEngine->CircleCircleCollision();
	physicsEngine->AABBAABBCollision();
	physicsEngine->CircleAABBCollision();

	m_playerSelected = (Util::Distance(EventManager::Instance().GetMousePosition(),
		m_pProjectile->GetTransform()->position) < m_pProjectile->GetWidth()) ? true : false;

	if (EventManager::Instance().GetMouseButton(0) && !EventManager::Instance().MouseReleased(1) && m_playerSelected)
	{
		m_pProjectile->GetTransform()->position = EventManager::Instance().GetMousePosition();

		auto distanceMouse = Util::Distance(EventManager::Instance().GetMousePosition(),
			starting_point);

		auto distanceBird = Util::Distance(m_pProjectile->GetTransform()->position, starting_point);

		float angle = Util::Angle360(m_pProjectile->GetTransform()->position, starting_point);

		if (distanceMouse > 75 && distanceBird >= 75)
		{
			m_pProjectile->GetTransform()->position = BirdPosPreviousFrame;
		}

		BirdPosPreviousFrame = m_pProjectile->GetTransform()->position;
	}
	else if (!EventManager::Instance().GetMouseButton(0) && EventManager::Instance().MouseReleased(1))
	{
		glm::vec2 d = m_pProjectile->GetTransform()->position - starting_point;

		physicsEngine->SetOnSlingshot(false);

		m_pProjectile->GetRigidBody()->velocity += (-d * slingShotPower) / m_pProjectile->GetRigidBody()->mass;
	}

	if (EventManager::Instance().MousePressed(3))
	{
		physicsEngine->SetOnSlingshot(true);
		m_pProjectile->GetTransform()->position = starting_point;
		m_pProjectile->GetRigidBody()->velocity = { 0,0 };
		m_pProjectile->GetRigidBody()->isColliding = false;
	}
	
	if (m_pSmallPig->GetRigidBody()->wasKilled)
	{
		score += m_pSmallPig->GetPoints();
		m_pSmallPig->GetRigidBody()->wasKilled = false;
		physicsEngine->RemoveCircleObject(m_pSmallPig->GetRigidBody());
		m_pSmallPig->SetEnabled(false);
		m_pSmallRemoved = true;
	}

	if (m_pMediumPig->GetRigidBody()->wasKilled)
	{
		score += m_pMediumPig->GetPoints();
		m_pMediumPig->GetRigidBody()->wasKilled = false;
		physicsEngine->RemoveCircleObject(m_pMediumPig->GetRigidBody());
		m_pMediumPig->SetEnabled(false);
		m_pMediumRemoved = true;
	}

	if (m_pBigPig->GetRigidBody()->wasKilled)
	{
		score += m_pBigPig->GetPoints();
		m_pBigPig->GetRigidBody()->wasKilled = false;
		physicsEngine->RemoveCircleObject(m_pBigPig->GetRigidBody());
		m_pBigPig->SetEnabled(false);
		m_pBigRemoved = true;
	}


	std::stringstream sst;
	sst << "Score: ";
	sst << score;
	m_pScoreLabel->SetText(sst.str());

	std::stringstream sst2;
	sst2 << "Use Left-Mouse to drag the bird and release to shoot. Click Right-Mouse to reload the bird.";
	m_pInstructionLabel->SetText(sst2.str());

	std::stringstream sst3;
	sst3 << "1 & 2 switch the bird.Space to reset the game";
	m_pInstructionLabel2->SetText(sst3.str());


}




void PlayScene::Clean()
{
	RemoveAllChildren();
}


void PlayScene::HandleEvents()
{
	EventManager::Instance().Update();

	GetKeyboardInput();
}

void PlayScene::GetKeyboardInput()
{
	if (EventManager::Instance().IsKeyDown(SDL_SCANCODE_ESCAPE))
	{
		Game::Instance().Quit();
	}

	if (EventManager::Instance().IsKeyDown(SDL_SCANCODE_1))
	{
		if (!m_pBird->GetRigidBody()->isActive)
		{
			m_pProjectile = m_pBird;
			m_pProjectile->GetTransform()->position = starting_point;
			m_pSquareBird->GetTransform()->position = idle_point;
			m_pBird->GetRigidBody()->isActive = true;
			m_pSquareBird->GetRigidBody()->isActive = false;
			physicsEngine->SetOnSlingshot(true);
		}
	}

	if (EventManager::Instance().IsKeyDown(SDL_SCANCODE_2))
	{
		if (!m_pSquareBird->GetRigidBody()->isActive)
		{
			m_pProjectile = m_pSquareBird;
			m_pProjectile->GetTransform()->position = starting_point;
			m_pBird->GetTransform()->position = idle_point;
			m_pSquareBird->GetRigidBody()->isActive = true;
			m_pBird->GetRigidBody()->isActive = false;
			physicsEngine->SetOnSlingshot(true);
		}
	}

	if (EventManager::Instance().IsKeyDown(SDL_SCANCODE_SPACE))
	{
		score = 0;
		if (m_pSmallRemoved)
		{
			m_pSmallRemoved = false;
			m_pSmallPig->GetTransform()->position = { 580, 172 };
			m_pSmallPig->SetEnabled(true);
			physicsEngine->AddCircleObject(m_pSmallPig->GetRigidBody());
		}

		if (m_pMediumRemoved)
		{
			m_pMediumRemoved = false;
			m_pMediumPig->GetTransform()->position = { 580, 450 };
			m_pMediumPig->SetEnabled(true);
			physicsEngine->AddCircleObject(m_pMediumPig->GetRigidBody());
		}

		if (m_pBigRemoved)
		{
			m_pBigRemoved = false;
			m_pBigPig->GetTransform()->position = { 780, 444 };
			m_pBigPig->SetEnabled(true);
			physicsEngine->AddCircleObject(m_pBigPig->GetRigidBody());
		}

		m_pLongBlock->GetTransform()->position = { 573, 211 };
		m_pBlock6->GetTransform()->position = { 696, 272 };
		m_pBlock5->GetTransform()->position = { 696, 363 };
		m_pBlock4->GetTransform()->position = { 696, 454 };
		m_pBlock3->GetTransform()->position = { 450, 272 };
		m_pBlock2->GetTransform()->position = { 450, 363 };
		m_pBlock->GetTransform()->position = { 450, 454 };
	}

	if (EventManager::Instance().KeyPressed(SDL_SCANCODE_H))
	{
		if (m_pDrawHalfplane == true)
		{
			m_pDrawHalfplane = false;
			m_pItIsDrew = false;
		}
		else
		{
			m_pDrawHalfplane = true;
		}
	}
}

void PlayScene::Start()
{
	// Set GUI Title
	m_guiTitle = "Play Scene";
	const SDL_Color color = { 255.0, 100.0, 50.0, 255.0 };
	m_pScoreLabel =  new Label("", "Consolas", 30, color, { 600, 30 });
	AddChild(m_pScoreLabel);

	m_pInstructionLabel = new Label("", "Consolas", 15, color, { 500, 60 });
	AddChild(m_pInstructionLabel);

	m_pInstructionLabel2 = new Label("", "Consolas", 15, color, { 500, 80 });
	AddChild(m_pInstructionLabel2);

	TextureManager::Instance().Load("../Assets/textures/background.png", "background");

	m_pBird = new Bird(45, 45);
	m_pBird->GetTransform()->position = starting_point;
	m_pBird->GetRigidBody()->radius = 22;
	m_pBird->GetRigidBody()->friction = 0.1;
	m_pBird->GetRigidBody()->mass = 500.0;
	m_pBird->GetRigidBody()->restitution = 0.9;
	AddChild(m_pBird);

	m_pSquareBird = new SquareBird(50, 50);
	m_pSquareBird->GetTransform()->position = idle_point;
	m_pSquareBird->GetRigidBody()->radius = 22;
	m_pSquareBird->GetRigidBody()->friction = 0.1;
	m_pSquareBird->GetRigidBody()->mass = 700.0;
	m_pSquareBird->GetRigidBody()->restitution = 0.9;
	AddChild(m_pSquareBird);

	m_pSmallPig = new SmallPig(48,48);
	m_pSmallPig->GetTransform()->position = { 580, 172 };
	m_pSmallPig->GetRigidBody()->mass = 3000;
	m_pSmallPig->GetRigidBody()->restitution = 0.9;
	AddChild(m_pSmallPig);

	m_pMediumPig = new MediumPig(80,80);
	m_pMediumPig->GetTransform()->position = { 580, 450 };
	m_pMediumPig->GetRigidBody()->mass = 4000;
	m_pMediumPig->GetRigidBody()->restitution = 0.9;
	AddChild(m_pMediumPig);

	m_pBlock = new Block(55,90);
	m_pBlock->GetTransform()->position = { 450, 454 };
	m_pBlock->GetRigidBody()->mass = 4000;
	m_pBlock->GetRigidBody()->restitution = 0.9;
	m_pBlock->GetRigidBody()->friction = 0.9;
	AddChild(m_pBlock);

	m_pBlock2 = new Block(55, 90);
	m_pBlock2->GetTransform()->position = { 450, 363 };
	m_pBlock2->GetRigidBody()->mass = 4000;
	m_pBlock2->GetRigidBody()->restitution = 0.9;
	AddChild(m_pBlock2);

	m_pBlock3 = new Block(55, 90);
	m_pBlock3->GetTransform()->position = { 450, 272 };
	m_pBlock3->GetRigidBody()->mass = 4000;
	m_pBlock3->GetRigidBody()->restitution = 0.9;
	AddChild(m_pBlock3);

	m_pBlock4 = new Block(55, 90);
	m_pBlock4->GetTransform()->position = { 696, 454 };
	m_pBlock4->GetRigidBody()->mass = 4000;
	m_pBlock4->GetRigidBody()->restitution = 0.9;
	AddChild(m_pBlock4);

	m_pBlock5 = new Block(55, 90);
	m_pBlock5->GetTransform()->position = { 696, 363 };
	m_pBlock5->GetRigidBody()->mass = 4000;
	m_pBlock5->GetRigidBody()->restitution = 0.9;
	AddChild(m_pBlock5);

	m_pBlock6 = new Block(55, 90);
	m_pBlock6->GetTransform()->position = { 696, 272 };
	m_pBlock6->GetRigidBody()->mass = 4000;
	m_pBlock6->GetRigidBody()->restitution = 0.9;
	AddChild(m_pBlock6);

	m_pLongBlock = new LongBlock(300, 30);
	m_pLongBlock->GetTransform()->position = { 573, 211 };
	m_pLongBlock->GetRigidBody()->mass = 4000;
	m_pLongBlock->GetRigidBody()->restitution = 0.9;
	AddChild(m_pLongBlock);

	m_pGround = new Ground(100000, 125);
	m_pGround->GetTransform()->position = { 505, 565 };
	m_pGround->GetRigidBody()->mass = 400;
	m_pGround->GetRigidBody()->restitution = 0.9;
	m_pGround->GetRigidBody()->enableGravity = false;
	AddChild(m_pGround);

	m_pBigPig = new BigPig(98,98);
	m_pBigPig->GetTransform()->position = { 780, 444 };
	m_pBigPig->GetRigidBody()->mass = 5000;
	m_pBigPig->GetRigidBody()->restitution = 0.9;
	AddChild(m_pBigPig);

	// Add circle shaped objects
	physicsEngine = new PhysicsEngine();
	physicsEngine->AddCircleObject(m_pBird->GetRigidBody());
	physicsEngine->AddCircleObject(m_pSmallPig->GetRigidBody());
	physicsEngine->AddCircleObject(m_pMediumPig->GetRigidBody());
	physicsEngine->AddCircleObject(m_pBigPig->GetRigidBody());

	// Add rectangle shaped objects
	physicsEngine->AddRectangleObject(m_pSquareBird->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pBlock->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pBlock2->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pBlock3->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pBlock4->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pBlock5->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pBlock6->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pLongBlock->GetRigidBody());
	physicsEngine->AddRectangleObject(m_pGround->GetRigidBody());

	m_pProjectile = m_pBird;
	m_pBird->GetRigidBody()->isActive = true;

	/* DO NOT REMOVE */
	ImGuiWindowFrame::Instance().SetGuiFunction([this] { GUI_Function(); });
}

void PlayScene::GUI_Function()
{
	// Always open with a NewFrame
	ImGui::NewFrame();


	ImGui::Begin("Your Window Title Goes Here", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove);

	if (ImGui::Button("My Button"))
	{
		std::cout << "My Button Pressed" << std::endl;
	}

	static float float3[3] = { 0.0f, 1.0f, 1.5f };
	if (ImGui::SliderFloat3("My Slider", float3, 0.0f, 2.0f))
	{
		std::cout << float3[0] << std::endl;
		std::cout << float3[1] << std::endl;
		std::cout << float3[2] << std::endl;
		std::cout << "---------------------------\n";
	}

	ImGui::Separator();

	ImGui::SliderFloat("Lunch Angle", &lunchAngle, 1.0f, 360.0f);

	ImGui::Separator();

	ImGui::SliderFloat("Lunch Speed", &lunchSpeed, 700.0f, 1200.0f);

	ImGui::Separator();

	ImGui::SliderFloat("Mass 1", &m_pBird->GetRigidBody()->mass, 1.0f, 1000.0f);

	ImGui::Separator();

	ImGui::SliderFloat("Sling Shot Power", &slingShotPower, 0, 1000);

	ImGui::Separator();

	ImGui::SliderFloat("Bounciness 1", &m_pBird->GetRigidBody()->restitution, 0.01f, 0.99f);

	ImGui::Separator();

	ImGui::SliderFloat("Gravity Acceleration", &accelerationGravity, -500.0f, -3000.0f);

	ImGui::Separator();

	ImGui::SliderFloat("Air - Friction", &friction, 0.9f, 1.0f);
	
	ImGui::End();
}