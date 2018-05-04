//
//  ColorCamera.h
//  Stonefish
//
//  Created by Patryk Cieslak on 4/5/18.
//  Copyright (c) 2018 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_ColorCamera__
#define __Stonefish_ColorCamera__

#include <functional>
#include "Camera.h"
#include "OpenGLRealCamera.h"

class ColorCamera : public Camera
{
public:
    ColorCamera(std::string uniqueName, uint32_t resX, uint32_t resY, btScalar horizFOVDeg, const btTransform& geomToSensor, SolidEntity* attachment = NULL, btScalar frequency = btScalar(-1.), uint32_t spp = 1, bool ao = true);
    virtual ~ColorCamera();
    
    void SetupCamera(const btVector3& eye, const btVector3& dir, const btVector3& up);
    void InstallNewDataHandler(std::function<void(ColorCamera*)> callback);
    void NewDataReady();
    
    //Sensor
	void InternalUpdate(btScalar dt);

private:
    OpenGLRealCamera* glCamera;
    std::function<void(ColorCamera*)> newDataCallback;
};

#endif