//
//  StaticEntity.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 24/05/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#include "StaticEntity.h"
#include "GeometryUtil.hpp"

StaticEntity::StaticEntity(std::string uniqueName, Material* mat, int _lookId) : Entity(uniqueName)
{
    material = mat;
	objectId = -1;
    lookId = _lookId;
    wireframe = false;
	rigidBody = NULL;
	mesh = NULL;
}

StaticEntity::~StaticEntity()
{
	if(mesh != NULL)
	{
		delete mesh;
		mesh = NULL;
	}
	
    material = NULL;
    rigidBody = NULL;
}

EntityType StaticEntity::getType()
{
    return ENTITY_STATIC;
}

void StaticEntity::GetAABB(btVector3& min, btVector3& max)
{
	if(rigidBody != NULL)
		rigidBody->getAabb(min, max);
}

void StaticEntity::Render()
{
    if(rigidBody != NULL && objectId >= 0 && isRenderable())
    {
		btTransform trans;
        rigidBody->getMotionState()->getWorldTransform(trans);
		OpenGLContent::getInstance()->DrawObject(objectId, lookId, glMatrixFromBtTransform(trans));
    }
}

Material* StaticEntity::getMaterial()
{
    return material;
}

void StaticEntity::AddToDynamicsWorld(btMultiBodyDynamicsWorld *world)
{
    world->addRigidBody(rigidBody, MASK_STATIC, MASK_DEFAULT);
}

btRigidBody* StaticEntity::getRigidBody()
{
    return rigidBody;
}

void StaticEntity::SetLook(int newLookId)
{
    lookId = newLookId;
}

void StaticEntity::SetWireframe(bool enabled)
{
    wireframe = enabled;
}