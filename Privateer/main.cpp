#include <irrlicht.h>
#include <irrKlang.h>
#include <iostream>
#include <vector>

using namespace irr;
using namespace irrklang;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

#ifdef _IRR_WINDOWS_
#pragma comment(lib, "Irrlicht.lib")
#pragma comment(lib, "irrKlang.lib")
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif


 //--- set camera to behave as cockpit camera of ship ---
void makeCockpit(irr::scene::ICameraSceneNode *camera, //camera
	irr::scene::ISceneNode *node, //scene node (ship)
	irr::core::vector3df offset) //relative position of camera to node (ship)
{
	//get rotation matrix of node - Zeuss must be getRotation not getRelativeTransformation
	irr::core::matrix4 m;
	m.setRotationDegrees(node->getRotation());

	// transform forward vector of camera
	irr::core::vector3df frv = irr::core::vector3df(0.0f, 0.0f, 1.0f);
	m.transformVect(frv);

	// transform upvector of camera
	irr::core::vector3df upv = irr::core::vector3df(0.0f, 1.0f, 0.0f);
	m.transformVect(upv);

	// transform camera offset (thanks to Zeuss for finding it was missing)
	m.transformVect(offset);

	// set camera
	camera->setPosition(node->getPosition() + offset); //position camera in front of the ship
	camera->setUpVector(upv); //set up vector of camera >> Zeuss - tested with +node->getPostion() and it didnt work, but this works fine.
	camera->setTarget(node->getPosition() + frv); //set target of camera (look at point) >> Zeuss - Dont forget to add the node positiob
}

void rotate(irr::scene::ISceneNode* node, irr::core::vector3df rot)
{
	irr::core::vector3df currentRot = node->getRotation();
	node->setRotation(currentRot + rot);
	node->updateAbsolutePosition();
}

//--- turn ship left or right ---
void turn(irr::scene::ISceneNode *node, irr::f32 rot)
{
	rotate(node, irr::core::vector3df(0.0f, rot, 0.0f));
}

//--- pitch ship up or down ---
void pitch(irr::scene::ISceneNode *node, irr::f32 rot)
{
	rotate(node, irr::core::vector3df(rot, 0.0f, 0.0f));
}

//--- roll ship left or right ---
void roll(irr::scene::ISceneNode *node, irr::f32 rot)
{
	rotate(node, irr::core::vector3df(0.0f, 0.0f, rot));
}

void move(irr::scene::ISceneNode *node, irr::core::vector3df vel)
{
	irr::core::matrix4 m;
	m.setRotationDegrees(node->getRotation());
	m.transformVect(vel);
	node->setPosition(node->getPosition() + vel);
	node->updateAbsolutePosition();
}

class MyEventReceiver : public IEventReceiver
{
public:


	// We'll create a struct to record info on the mouse state
	struct SMouseState
	{
		core::position2di Position;
		bool LeftButtonDown;
		SMouseState() : LeftButtonDown(false) { }
	} MouseState;


	// This is the one method that we have to implement
	virtual bool OnEvent(const SEvent& event)
	{
		// Remember whether each key is down or up
		if (event.EventType == irr::EET_KEY_INPUT_EVENT)
			KeyIsDown[event.KeyInput.Key] = event.KeyInput.PressedDown;

		if (event.EventType == irr::EET_MOUSE_INPUT_EVENT)
		{
			switch (event.MouseInput.Event)
			{
			case EMIE_LMOUSE_PRESSED_DOWN:
				MouseState.LeftButtonDown = true;
				break;

			case EMIE_LMOUSE_LEFT_UP:
				MouseState.LeftButtonDown = false;
				break;

			case EMIE_MOUSE_MOVED:
				MouseState.Position.X = event.MouseInput.X;
				MouseState.Position.Y = event.MouseInput.Y;
				break;

			default:
				// We won't use the wheel
				break;
			}
		}

		return false;
	}
	const SMouseState & GetMouseState(void) const
	{
		return MouseState;
	}
	// This is used to check whether a key is being held down
	virtual bool IsKeyDown(EKEY_CODE keyCode) const
	{
		return KeyIsDown[keyCode];
	}

	MyEventReceiver()
	{
		for (u32 i = 0; i < KEY_KEY_CODES_COUNT; ++i)
			KeyIsDown[i] = false;
	}

private:
	// We use this array to store the current state of each key
	bool KeyIsDown[KEY_KEY_CODES_COUNT];
};

// This is the movement speed in units per second.
f32 MOVEMENT_SPEED = 0.f;
f32 MOVEMENT_SPEED_TARGET = 0.f;

f32 MOVEMENT_SPEED_INCREMENT = 80.f;

f32 MOVEMENT_SPEED_DECEL = 60.f;

f32 MOVEMENT_SPEED_MAX_STANDARD = 288.f;
f32 MOVEMENT_SPEED_MAX_AFTERBURN = 576.f;

f32 MOVEMENT_SPEED_ACCEL = 50.f;
f32 MOVEMENT_SPEED_ACCEL_STANDARD = 50.f;
f32 MOVEMENT_SPEED_ACCEL_AFTERBRUN = 100.f;

f32 MOVEMENT_TURN = 0.f;
f32 MOVEMENT_TURN_MAX = 100.0f;

f32 MOVEMENT_PITCH = 0.f;
f32 MOVEMENT_PITCH_MAX = 100.0f;

u16 DISPLAY_WIDTH = 800;
u16 DISPLAY_HEIGHT = 600;
u16 MOUSE_DEAD = 10;
u16 MOUSE_MAX = 100;
u16 MOUSE_BOUNCE = 1;

int main()
{
	// create device
	MyEventReceiver receiver;

	IrrlichtDevice *device =
		createDevice( video::EDT_OPENGL, dimension2d<u32>(DISPLAY_WIDTH, DISPLAY_HEIGHT), 32,
			false, false, false, &receiver);

	if (!device)
	{
		printf("Could not start graphics engine\n");
		return 0;
	}

	ISoundEngine* snddevice = createIrrKlangDevice(ESOD_AUTO_DETECT);

	if (!snddevice)
	{
		printf("Could not start sound engine\n");
		return 0;
	}


	device->setWindowCaption(L"Privateer");

	IVideoDriver* driver = device->getVideoDriver();
	ISceneManager* smgr = device->getSceneManager();
	IGUIEnvironment* guienv = device->getGUIEnvironment();

	snddevice->setSoundVolume(0.3f);
	snddevice->play2D("assets/sounds/startup1.ogg", false);

	ISound* shipengine = snddevice->play2D("assets/sounds/engine1.ogg", true,true);
	ISound* shipengineafterburn = snddevice->play2D("assets/sounds/engine2.ogg", true,true);

	//set other font
	guienv->getSkin()->setFont(guienv->getFont("assets/fonts/default.bmp"));
	guienv->getSkin()->setColor(gui::EGDC_BUTTON_TEXT,
		video::SColor(255, 217, 206, 56));
	IGUIStaticText* textTargetSpeed = guienv->addStaticText(L"XXX", rect<s32>(0, 0, 150, 20), false);
	IGUIStaticText* textSpeed = guienv->addStaticText(L"XXX", rect<s32>(0, 20, 150, 40), false);

	/* Sky Dome */
	/**********************************************************************************************/
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	scene::ISceneNode* skydome = smgr->addSkyDomeSceneNode(driver->getTexture("assets/space/nebula1.png"), 16, 8, 0.95f, 2.0f);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, true);
	/**********************************************************************************************/

	/* Test Planet */
	/**********************************************************************************************/
	IAnimatedMesh* planet = smgr->getMesh("assets/models/planets/planet.obj");
	if (!planet)
	{
		device->drop();
		return 1;
	}
	IAnimatedMeshSceneNode* planetnode = smgr->addAnimatedMeshSceneNode(planet,skydome);
	if (planetnode)
	{
		core::vector3df scale;
		scale.X = 5000;
		scale.Y = 5000;
		scale.Z = 5000;
		planetnode->setScale(scale);
		planetnode->setPosition(core::vector3df(0, 0, 50000));
		planetnode->setMaterialFlag(EMF_LIGHTING, false);
		planetnode->setMaterialTexture(0, driver->getTexture("assets/models/planets/earth.png"));
	}
	/**********************************************************************************************/

	/*  Test Asteroid */
	/**********************************************************************************************/
	IAnimatedMesh* asteroid = smgr->getMesh("assets/models/junk/asteroid.obj");
	if (!asteroid)
	{
		device->drop();
		return 1;
	}
	IAnimatedMeshSceneNode* asteroidnode = smgr->addAnimatedMeshSceneNode(asteroid,skydome);
	if (!asteroidnode)
	{
		device->drop();
		return 1;
	}
	else
	{
		core::vector3df scale;
		scale.X = 2;
		scale.Y = 2;
		scale.Z = 2;
		asteroidnode->setScale(scale);
		asteroidnode->setPosition(core::vector3df(200, 0, 500));
		asteroidnode->setMaterialFlag(EMF_LIGHTING, false);
		asteroidnode->setMaterialTexture(0, driver->getTexture("assets/models/junk/asteroid.png"));
	}
	/**********************************************************************************************/

	video::ITexture* sightimages = driver->getTexture("assets/hud/sight_all.png");

	// Dust nodse
	std::vector<IAnimatedMeshSceneNode*> dustnodes;

	// Space dust model
	IAnimatedMesh* dust = smgr->getMesh("assets/models/junk/dust.obj");
	if (!dust)
	{
		device->drop();
		return 1;
	}
	else
	{
		// Create 500 dust nodes
		for (u16 i = 0; i < 500; ++i)
		{
			IAnimatedMeshSceneNode* dustNode = smgr->addAnimatedMeshSceneNode(dust);
			if (dustNode != nullptr) {
				dustnodes.push_back(dustNode);
			}
		}

		// Place them far away so they get re-arranged later
		for (size_t i = 0; i < dustnodes.size(); ++i)
		{
			core::vector3df scale;
			scale.X = 0.2f;
			scale.Y = 0.2f;
			scale.Z = 0.2f;
			dustnodes[i]->setScale(scale);
			dustnodes[i]->setPosition(core::vector3df(1000, 1000, 1000));
			dustnodes[i]->setMaterialFlag(EMF_LIGHTING, false);
			dustnodes[i]->setMaterialTexture(0, driver->getTexture("assets/models/junk/dust.png"));
		}
	}

	/* Player shield*/
	/**********************************************************************************************/
	IAnimatedMesh* shieldmesh = smgr->getMesh("assets/models/shield.obj");
	if (!shieldmesh)
	{
		device->drop();
		return 1;
	}
	IAnimatedMeshSceneNode* shieldnode = smgr->addAnimatedMeshSceneNode(shieldmesh);
	if (!shieldnode)
	{
		device->drop();
		return 1;
	}else{
		core::vector3df scale;
		scale.X = 4;
		scale.Y = 4;
		scale.Z = 4;
		shieldnode->setScale(scale);
		shieldnode->setMaterialFlag(EMF_LIGHTING, false);
		shieldnode->setMaterialType(EMT_TRANSPARENT_ALPHA_CHANNEL);
		shieldnode->setMaterialTexture(0, driver->getTexture("assets/models/shield.png"));
	}
	/**********************************************************************************************/


	scene::ICameraSceneNode* camera = smgr->addCameraSceneNode(0, vector3df(0,0,0), vector3df(0,0,1));
	camera->setFarValue(50000.0f);
	scene::ISceneNode* test = smgr->addSceneNode("empty");
	test->setPosition(vector3df(0, 0, 0));

	/* Store last FPS so we can show it if enabled*/
	u16 lastFPS = -1;

	device->getCursorControl()->setPosition(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2);
	device->getCursorControl()->setVisible(false);

	// In order to do framerate independent movement, we have to know
	// how long it was since the last frame
	u32 then = device->getTimer()->getTime();

	while (device->run())
	{
		// Check dust nodes and if any are too far away, reset them.
		for (size_t i = 0; i < dustnodes.size(); ++i)
		{
			core::vector3df dustpos = dustnodes[i]->getPosition();
			if (dustpos.getDistanceFrom(test->getPosition()) > (1.42 * 250))
			{
				core::vector3df newrandom = core::vector3df((float)(rand() % 500) - 250, (float)(rand() % 500) - 250, (float)(rand() % 500) - 250);
				dustnodes[i]->setPosition(test->getPosition());
				move(dustnodes[i], newrandom);
			}
		}

		// Work out a frame delta time.
		const u32 now = device->getTimer()->getTime();
		const f32 frameDeltaTime = (f32)(now - then) / 1000.f; // Time in seconds
		then = now;

		core::vector3df target = (camera->getTarget() - camera->getAbsolutePosition());
		core::vector3df relativeRotation = target.getHorizontalAngle();

		if (receiver.IsKeyDown(irr::KEY_ESCAPE))
		{
			break;
		}
		else if (receiver.IsKeyDown(irr::KEY_PLUS))
		{
			MOVEMENT_SPEED_TARGET += MOVEMENT_SPEED_INCREMENT * frameDeltaTime;
			MOVEMENT_SPEED_TARGET = (MOVEMENT_SPEED_TARGET > MOVEMENT_SPEED_MAX_STANDARD) ? MOVEMENT_SPEED_MAX_STANDARD : MOVEMENT_SPEED_TARGET;
		}
		else if (receiver.IsKeyDown(irr::KEY_MINUS))
		{
			MOVEMENT_SPEED_TARGET -= MOVEMENT_SPEED_INCREMENT * frameDeltaTime;
			MOVEMENT_SPEED_TARGET = (MOVEMENT_SPEED_TARGET < 0) ? 0 : MOVEMENT_SPEED_TARGET;
		}
		else if (receiver.IsKeyDown(irr::KEY_OEM_4))
		{
			MOVEMENT_SPEED_TARGET = 0;
		}
		else if (receiver.IsKeyDown(irr::KEY_OEM_6))
		{
			MOVEMENT_SPEED_TARGET = MOVEMENT_SPEED_MAX_STANDARD;
		}
		else if (receiver.IsKeyDown(irr::KEY_TAB))
		{
			MOVEMENT_SPEED_ACCEL = MOVEMENT_SPEED_ACCEL_AFTERBRUN;
			MOVEMENT_SPEED_TARGET = MOVEMENT_SPEED_MAX_AFTERBURN;
			shipengine->setIsPaused(true);
			shipengineafterburn->setIsPaused(false);
		}
		else {
			MOVEMENT_SPEED_ACCEL = MOVEMENT_SPEED_ACCEL_STANDARD;
			MOVEMENT_SPEED_TARGET = (MOVEMENT_SPEED_TARGET > MOVEMENT_SPEED_MAX_STANDARD) ? MOVEMENT_SPEED_MAX_STANDARD : MOVEMENT_SPEED_TARGET;
			shipengine->setIsPaused(false);
			shipengineafterburn->setIsPaused(true);
		}

		if (device->getCursorControl()->getPosition().X > ((DISPLAY_WIDTH/2)+MOUSE_DEAD))
		{
			MOVEMENT_TURN = ((device->getCursorControl()->getPosition().X - ((DISPLAY_WIDTH / 2) + MOUSE_DEAD))*1.1f);
			if((receiver.GetMouseState().Position.X > ((DISPLAY_WIDTH/2) + MOUSE_MAX)))
				device->getCursorControl()->setPosition((DISPLAY_WIDTH/2)+MOUSE_MAX-MOUSE_BOUNCE, device->getCursorControl()->getPosition().Y);

		}
		else if (device->getCursorControl()->getPosition().X < (DISPLAY_WIDTH/2)-MOUSE_DEAD)
		{
			MOVEMENT_TURN = 0 -(((DISPLAY_WIDTH/2)-MOUSE_DEAD) - device->getCursorControl()->getPosition().X)*1.1f;
			if ((receiver.GetMouseState().Position.X < (DISPLAY_WIDTH/2)-MOUSE_MAX))
				device->getCursorControl()->setPosition((DISPLAY_WIDTH / 2)-MOUSE_MAX+MOUSE_BOUNCE, device->getCursorControl()->getPosition().Y);
		}
		else
		{
			MOVEMENT_TURN = 0;
		}

		if (device->getCursorControl()->getPosition().Y < ((DISPLAY_HEIGHT/2)))
		{
			MOVEMENT_PITCH = ((static_cast<irr::f32>(DISPLAY_HEIGHT/2)) - device->getCursorControl()->getPosition().Y);
			if ((receiver.GetMouseState().Position.Y < (DISPLAY_HEIGHT / 2)-MOUSE_MAX))
				device->getCursorControl()->setPosition(device->getCursorControl()->getPosition().X, (DISPLAY_HEIGHT/2)-MOUSE_MAX+MOUSE_BOUNCE);
		} 
		else if (device->getCursorControl()->getPosition().Y > (DISPLAY_HEIGHT/2))
		{
			MOVEMENT_PITCH = 0-((device->getCursorControl()->getPosition().Y - (static_cast<irr::f32>(DISPLAY_HEIGHT / 2))));
			if ((receiver.GetMouseState().Position.Y > (DISPLAY_HEIGHT/2)+MOUSE_MAX))
				device->getCursorControl()->setPosition(device->getCursorControl()->getPosition().X, (DISPLAY_HEIGHT/2)+MOUSE_MAX-MOUSE_BOUNCE);
		}
		else
		{
				MOVEMENT_PITCH = 0;
		}

		if (MOVEMENT_SPEED > 0)
		{
			pitch(test, ((MOVEMENT_PITCH / MOUSE_MAX * MOVEMENT_PITCH_MAX)) * frameDeltaTime);
			turn(test, ((MOVEMENT_TURN / MOUSE_MAX * MOVEMENT_TURN_MAX)) * frameDeltaTime);
		}
		else {
			device->getCursorControl()->setPosition((DISPLAY_WIDTH/2), (DISPLAY_HEIGHT/2));
		}
		
		/* Tend towards desired speed */
		if (MOVEMENT_SPEED > MOVEMENT_SPEED_TARGET)
			MOVEMENT_SPEED -= MOVEMENT_SPEED_DECEL * frameDeltaTime;
		if (MOVEMENT_SPEED < MOVEMENT_SPEED_TARGET)
			MOVEMENT_SPEED += MOVEMENT_SPEED_ACCEL * frameDeltaTime;
		move(test, vector3df(0, 0,MOVEMENT_SPEED * frameDeltaTime));

		stringw strspeed = L"Current velocity: ";
		strspeed += (int)round(MOVEMENT_SPEED);
		strspeed += L" m/s";
		textSpeed->setText(strspeed.c_str());

		stringw strtargetspeed = L"Target velocity: ";
		strtargetspeed += (int)round(MOVEMENT_SPEED_TARGET);
		strtargetspeed += L" m/s";
		textTargetSpeed->setText(strtargetspeed.c_str());

		makeCockpit(camera, test, vector3df(0, 0, 0));

		/* Render */
		/**********************************************************************************************/
		driver->beginScene(true, true, SColor(100, 100, 100, 100));

		smgr->drawAll();
		guienv->drawAll();

		// Crosshair
		driver->draw2DImage(sightimages, core::position2d<s32>((DISPLAY_WIDTH/2) - 41, (DISPLAY_HEIGHT / 2) - 41), core::rect<s32>(1, 1, 88, 74), 0, SColor(255, 255, 255, 255), true);

		// H
        driver->draw2DImage(
           sightimages, 
           core::position2d<s32>(
               static_cast<s32>((DISPLAY_WIDTH / 2) - (54 / 2)), 
               static_cast<s32>((DISPLAY_HEIGHT / 2) - 50)
           ), 
           core::rect<s32>(126, 1, 179, 7), 
           0, 
           SColor(255, 255, 255, 255), 
           true
        );

		driver->draw2DImage(
           sightimages, 
           core::position2d<s32>(
               static_cast<s32>((DISPLAY_WIDTH / 2) + ((MOVEMENT_TURN / MOUSE_MAX) * 26) - 2), 
               static_cast<s32>((DISPLAY_HEIGHT / 2) - 40)
           ), 
           core::rect<s32>(120, 1, 123, 6), 
           0, 
           SColor(255, 255, 255, 255), 
           true
        );

		// V
        driver->draw2DImage(  
           sightimages,  
           core::position2d<s32>(  
               static_cast<s32>((DISPLAY_WIDTH / 2) - 50),  
               static_cast<s32>((DISPLAY_HEIGHT / 2) - (52 / 2))  
           ),  
           core::rect<s32>(126, 10, 132, 63),  
           0,  
           SColor(255, 255, 255, 255),  
           true  
        );  

        driver->draw2DImage(  
           sightimages,  
           core::position2d<s32>(  
               static_cast<s32>((DISPLAY_WIDTH / 2) - 41),  
               static_cast<s32>((DISPLAY_HEIGHT / 2) + ((MOVEMENT_PITCH / MOUSE_MAX) * 26))  
           ),  
           core::rect<s32>(112, 1, 117, 4),  
           0,  
           SColor(255, 255, 255, 255),  
           true  
        );

		// Throttle
		// ???

		driver->endScene();
		/**********************************************************************************************/

		/* FPS Counter - TODO: Add cvar*/
		/**********************************************************************************************/
		u16 fps = driver->getFPS();
		if (lastFPS != fps)
		{
			core::stringw tmp(L"Privateer [");
			tmp += L"] fps: ";
			tmp += fps;

			device->setWindowCaption(tmp.c_str());
			lastFPS = fps;
		}
		/**********************************************************************************************/

		shieldnode->setPosition(camera->getAbsolutePosition());

		asteroidnode->setDebugDataVisible(scene::EDS_BBOX);
		planetnode->setDebugDataVisible(scene::EDS_BBOX);
		shieldnode->setDebugDataVisible(scene::EDS_BBOX);
		if (asteroidnode->getTransformedBoundingBox().intersectsWithBox(shieldnode->getTransformedBoundingBox()))
		{
				snddevice->play2D("assets/sounds/shldhit.ogg", false);
		}
	}

	device->drop();
	snddevice->drop(); // delete engine
	return 0;
}
