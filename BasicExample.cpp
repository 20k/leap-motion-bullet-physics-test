/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2015 Google Inc. http://bulletphysics.org

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#include "BasicExample.h"

#include <btBulletDynamicsCommon.h>
#define ARRAY_SIZE_Y 5
#define ARRAY_SIZE_X 5
#define ARRAY_SIZE_Z 5

#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

#include "CommonRigidBodyBase.h"
#include <vector>

//typedef void (*btInternalTickCallback)(btDynamicsWorld *world, btScalar timeStep);

void step_callback(btDynamicsWorld* world, btScalar timeStep)
{
    usr_world_info* info = (usr_world_info*)world->getWorldUserInfo();

    if(info == nullptr)
        return;

    info->internal_step_id++;
}

struct BasicExample : public CommonRigidBodyBase
{
    std::vector<btRigidBody*> rigid_bodies;
    //std::vector<btRigidBody*>

	BasicExample(struct GUIHelperInterface* helper)
		:CommonRigidBodyBase(helper)
	{
	}
	virtual ~BasicExample(){}
	virtual void initPhysics();
	virtual void renderScene();
	void resetCamera()
	{
		float dist = 41;
		float pitch = 52;
		float yaw = 35;
		float targetPos[3]={0,0.46,0};
		m_guiHelper->resetCamera(dist,pitch,yaw,targetPos[0],targetPos[1],targetPos[2]);
	}

	virtual void tick()
	{
        //printf("hi\n");

        /*for(int i=0; i<rigid_bodies.size(); i++)
        {
            btRigidBody* s = rigid_bodies[i];

            btTransform trans;

            s->getMotionState()->getWorldTransform(trans);

            m_dynamicsWorld->removeRigidBody(s);
            m_dynamicsWorld->addRigidBody(s);
        }*/



        /*for(btCollisionShape* s : m_collisionShapes)
        {
            s->getMotionState();
        }*/
	}

	virtual std::vector<btRigidBody*>* getBodies()
	{
	    return &rigid_bodies;
	}
};

void BasicExample::initPhysics()
{
	m_guiHelper->setUpAxis(1);

	createEmptyDynamicsWorld();

	m_guiHelper->createPhysicsDebugDrawer(m_dynamicsWorld);

	if (m_dynamicsWorld->getDebugDrawer())
		m_dynamicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_DrawWireframe+btIDebugDraw::DBG_DrawContactPoints);

	///create a few basic rigid bodies
	btBoxShape* groundShape = createBoxShape(btVector3(btScalar(500.),btScalar(50.),btScalar(500.)));


	//groundShape->initializePolyhedralFeatures();
//	btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0,1,0),50);

	m_collisionShapes.push_back(groundShape);

	btTransform groundTransform;
	groundTransform.setIdentity();
	groundTransform.setOrigin(btVector3(0,150,0));

	{
		btScalar mass(0.);
		auto rigid = createRigidBody(mass,groundTransform,groundShape, btVector4(0,0,1,1));

		rigid->setFriction(1.f);

		rigid_bodies.push_back(rigid);
	}


	{
		//create a few dynamic rigidbodies
		// Re-using the same collision is better for memory usage and performance

		float scale = 20.f;

		btBoxShape* colShape = createBoxShape(btVector3(scale,scale,scale));


		//btCollisionShape* colShape = new btSphereShape(btScalar(1.));
		m_collisionShapes.push_back(colShape);

		/// Create Dynamic Objects
		btTransform startTransform;
		startTransform.setIdentity();

		btScalar	mass(1.f);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0,0,0);
		if (isDynamic)
			colShape->calculateLocalInertia(mass,localInertia);


		for (int k=0;k<ARRAY_SIZE_Y;k++)
		{
			for (int i=0;i<ARRAY_SIZE_X;i++)
			{
				for(int j = 0;j<ARRAY_SIZE_Z;j++)
				{
					startTransform.setOrigin(btVector3(
										btScalar(2.0*i*scale),
										btScalar(300+2.0*k*scale),
										btScalar(2.0*j*scale)));


					btRigidBody* rigid = createRigidBody(mass,startTransform,colShape);

					rigid->setFriction(1.f);
					rigid->setRollingFriction(1.f);

                    rigid_bodies.push_back(rigid);
				}
			}
		}
	}

	m_guiHelper->autogenerateGraphicsObjects(m_dynamicsWorld);
}


void BasicExample::renderScene()
{
	CommonRigidBodyBase::renderScene();

}

CommonRigidBodyBase*    BasicExampleCreateFunc(CommonExampleOptions& options)
{
	return new BasicExample(options.m_guiHelper);
}



