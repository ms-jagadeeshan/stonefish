//
//  Light.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 4/7/17.
//  Copyright (c) 2017-2018 Patryk Cieslak. All rights reserved.
//

#include "actuators/Light.h"

#include "core/GraphicalSimulationApp.h"
#include "core/Console.h"
#include "graphics/OpenGLPipeline.h"
#include "graphics/OpenGLPointLight.h"
#include "graphics/OpenGLSpotLight.h"
#include "graphics/OpenGLContent.h"

namespace sf
{

Light::Light(std::string uniqueName, Color color, Scalar illuminance) : LinkActuator(uniqueName), c(color), coneAngle(0)
{
    if(!SimulationApp::getApp()->hasGraphics())
        cCritical("Not possible to use lights in console simulation! Use graphical simulation if possible.");
    
    illum = illuminance < Scalar(0) ? Scalar(0) : illuminance;
}

Light::Light(std::string uniqueName, Scalar coneAngleDeg, Color color, Scalar illuminance) :
    Light(uniqueName, color, illuminance)
{
    coneAngle = coneAngleDeg > Scalar(0) ? coneAngleDeg : Scalar(45);
}
    
ActuatorType Light::getType()
{
    return ActuatorType::ACTUATOR_LIGHT;
}

void Light::AttachToLink(FeatherstoneEntity* multibody, unsigned int linkId, const Transform& origin)
{
    LinkActuator::AttachToLink(multibody, linkId, origin);
    
    if(attach != NULL)
        InitGraphics();
}
    
void Light::AttachToSolid(SolidEntity* solid, const Transform& origin)
{
    LinkActuator::AttachToSolid(solid, origin);
    
    if(attach != NULL)
        InitGraphics();
}
   
void Light::InitGraphics()
{
    if(coneAngle > Scalar(0)) //Spot light
        glLight = new OpenGLSpotLight(glm::vec3(0.f), glm::vec3(0.f,0.f,-1.f), (GLfloat)coneAngle, c.rgb, (GLfloat)illum);
    else //Omnidirectional light
        glLight = new OpenGLPointLight(glm::vec3(0.f), c.rgb, (GLfloat)illum);
    
    UpdateTransform();
    glLight->UpdateTransform();
    ((GraphicalSimulationApp*)SimulationApp::getApp())->getGLPipeline()->getContent()->AddLight(glLight);
}
    
void Light::Update(Scalar dt)
{
}

void Light::UpdateTransform()
{
    Transform lightTransform = getActuatorFrame();
    Matrix3 rotation;
    rotation.setEulerYPR(0,M_PI,0);
    Vector3 pos = rotation * lightTransform.getOrigin();
    glm::vec3 glPos((GLfloat)pos.x(), (GLfloat)pos.y(), (GLfloat)pos.z());
    glLight->UpdatePosition(glPos);
    
    if(coneAngle > Scalar(0))
    {
        Vector3 dir = rotation * lightTransform.getBasis().getColumn(2);
        glm::vec3 glDir((GLfloat)dir.x(), (GLfloat)dir.y(), (GLfloat)dir.z());
        ((OpenGLSpotLight*)glLight)->UpdateDirection(glDir);
    }
}
    
std::vector<Renderable> Light::Render()
{
    std::vector<Renderable> items(0);
    
    Renderable item;
    item.model = glMatrixFromTransform(getActuatorFrame());
    item.type = RenderableType::ACTUATOR_LINES;
    
    GLfloat iconSize = 1.f;
    unsigned int div = 24;
    
    if(coneAngle > Scalar(0))
    {
        GLfloat r = iconSize * tanf((GLfloat)coneAngle/360.f*M_PI);
        
        for(unsigned int i=0; i<div; ++i)
        {
            GLfloat angle1 = (GLfloat)i/(GLfloat)div * 2.f * M_PI;
            GLfloat angle2 = (GLfloat)(i+1)/(GLfloat)div * 2.f * M_PI;
            item.points.push_back(glm::vec3(r * cosf(angle1), r * sinf(angle1), iconSize));
            item.points.push_back(glm::vec3(r * cosf(angle2), r * sinf(angle2), iconSize));
        }
        
        item.points.push_back(glm::vec3(0,0,0));
        item.points.push_back(glm::vec3(r, 0, iconSize));
        item.points.push_back(glm::vec3(0,0,0));
        item.points.push_back(glm::vec3(-r, 0, iconSize));
        item.points.push_back(glm::vec3(0,0,0));
        item.points.push_back(glm::vec3(0, r, iconSize));
        item.points.push_back(glm::vec3(0,0,0));
        item.points.push_back(glm::vec3(0, -r, iconSize));
    }
    else
    {
        for(unsigned int i=0; i<div; ++i)
        {
            GLfloat angle1 = (GLfloat)i/(GLfloat)div * 2.f * M_PI;
            GLfloat angle2 = (GLfloat)(i+1)/(GLfloat)div * 2.f * M_PI;
            item.points.push_back(glm::vec3(0.5f * iconSize * cosf(angle1), 0.5f * iconSize * sinf(angle1), 0));
            item.points.push_back(glm::vec3(0.5f * iconSize * cosf(angle2), 0.5f * iconSize * sinf(angle2), 0));
            item.points.push_back(glm::vec3(0.5f * iconSize * cosf(angle1), 0, 0.5f * iconSize * sinf(angle1)));
            item.points.push_back(glm::vec3(0.5f * iconSize * cosf(angle2), 0, 0.5f * iconSize * sinf(angle2)));
            item.points.push_back(glm::vec3(0, 0.5f * iconSize * cosf(angle1), 0.5f * iconSize * sinf(angle1)));
            item.points.push_back(glm::vec3(0, 0.5f * iconSize * cosf(angle2), 0.5f * iconSize * sinf(angle2)));
        }
    }
    
    items.push_back(item);
    
    return items;
}

}
