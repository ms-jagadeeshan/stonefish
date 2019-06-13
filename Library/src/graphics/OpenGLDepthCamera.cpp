/*    
    This file is a part of Stonefish.

    Stonefish is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Stonefish is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

//
//  OpenGLDepthCamera.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 04/05/18.
//  Copyright (c) 2019 Patryk Cieslak. All rights reserved.
//

#include "graphics/OpenGLDepthCamera.h"

#include "core/Console.h"
#include "core/GraphicalSimulationApp.h"
#include "entities/SolidEntity.h"
#include "sensors/vision/Camera.h"
#include "graphics/GLSLShader.h"
#include "graphics/OpenGLPipeline.h"
#include "graphics/OpenGLContent.h"

namespace sf
{

GLSLShader* OpenGLDepthCamera::depthLinearizeShader = NULL;
GLSLShader* OpenGLDepthCamera::depth2RangesShader = NULL;
GLSLShader* OpenGLDepthCamera::depthVisualizeShader = NULL;

OpenGLDepthCamera::OpenGLDepthCamera(glm::vec3 eyePosition, glm::vec3 direction, glm::vec3 cameraUp,
                                     GLint originX, GLint originY, GLint width, GLint height,
                                     GLfloat horizontalFOVDeg, GLfloat minDepth, GLfloat maxDepth,
                                     bool useRanges, GLfloat verticalFOVDeg)
 : OpenGLView(originX, originY, width, height)
{
    _needsUpdate = false;
    update = false;
    camera = NULL;
    idx = 0;
    range.x = minDepth;
    range.y = maxDepth;
    usesRanges = useRanges;
    
    SetupCamera(eyePosition, direction, cameraUp);
    UpdateTransform();
    
    GLfloat fovx = horizontalFOVDeg/180.f*M_PI;
    GLfloat fovy;
    
    if(verticalFOVDeg > 0.f)
        fovy = verticalFOVDeg/180.f*M_PI;
    else
        fovy = 2.f * atanf( (GLfloat)viewportHeight/(GLfloat)viewportWidth * tanf(fovx/2.f) );
    
    projection = glm::perspective(fovy, fovx/fovy, range.x, range.y);
    
    //Render depth
    glGenTextures(1, &renderDepthTex);
	glBindTexture(GL_TEXTURE_2D, renderDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, viewportWidth, viewportHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &renderFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, renderDepthTex, 0);
    glReadBuffer(GL_NONE);
    glDrawBuffer(GL_NONE);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
		cError("Depth Render FBO initialization failed!");
    
    //Linear depth
	glGenTextures(1, &linearDepthTex);
	glBindTexture(GL_TEXTURE_2D, linearDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, viewportWidth, viewportHeight, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture (GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &linearDepthFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, linearDepthFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, linearDepthTex, 0);
		
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
		cError("Linear Depth FBO initialization failed!");
}

OpenGLDepthCamera::~OpenGLDepthCamera()
{
    glDeleteTextures(1, &renderDepthTex);
	glDeleteFramebuffers(1, &renderFBO);
    glDeleteTextures(1, &linearDepthTex);
    glDeleteFramebuffers(1, &linearDepthFBO);
}

void OpenGLDepthCamera::SetupCamera(glm::vec3 _eye, glm::vec3 _dir, glm::vec3 _up)
{
	tempDir = _dir;
	tempEye = _eye;
	tempUp = _up;
}

void OpenGLDepthCamera::UpdateTransform()
{
    eye = tempEye;
    dir = tempDir;
    up = tempUp;
    SetupCamera();
}

void OpenGLDepthCamera::SetupCamera()
{
	cameraTransform = glm::lookAt(eye, eye+dir, up);
}

glm::vec3 OpenGLDepthCamera::GetEyePosition() const
{
    return eye;
}

glm::vec3 OpenGLDepthCamera::GetLookingDirection() const
{
    return dir;
}

glm::vec3 OpenGLDepthCamera::GetUpDirection() const
{
    return up;
}

glm::mat4 OpenGLDepthCamera::GetProjectionMatrix() const
{
    return projection;
}

glm::mat4 OpenGLDepthCamera::GetViewMatrix() const
{
    return cameraTransform;
}

void OpenGLDepthCamera::Update()
{
    _needsUpdate = true;
}

bool OpenGLDepthCamera::needsUpdate()
{
    update = _needsUpdate;
    _needsUpdate = false;
    return update && enabled;
}

void OpenGLDepthCamera::setCamera(Camera* cam, unsigned int index)
{
    camera = cam;
    idx = index;
}

ViewType OpenGLDepthCamera::getType()
{
    return DEPTH_CAMERA;
}

void OpenGLDepthCamera::LinearizeDepth()
{
    glBindFramebuffer(GL_FRAMEBUFFER, linearDepthFBO);
    glViewport(0, 0, viewportWidth, viewportHeight);
    glActiveTexture(GL_TEXTURE0 + TEX_POSTPROCESS1);
    glBindTexture(GL_TEXTURE_2D, renderDepthTex);
    
    depthLinearizeShader->Use();
	depthLinearizeShader->SetUniform("clipInfo", glm::vec4(range.x*range.y, range.x-range.y, range.y, 1.f));
	depthLinearizeShader->SetUniform("texDepth", TEX_POSTPROCESS1);
    ((GraphicalSimulationApp*)SimulationApp::getApp())->getGLPipeline()->getContent()->DrawSAQ();
    glUseProgram(0);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
    
void OpenGLDepthCamera::Depth2LinearRanges()
{
    glBindFramebuffer(GL_FRAMEBUFFER, linearDepthFBO);
    glViewport(0, 0, viewportWidth, viewportHeight);
    glActiveTexture(GL_TEXTURE0 + TEX_POSTPROCESS1);
    glBindTexture(GL_TEXTURE_2D, renderDepthTex);
    
    glm::mat4 proj = GetProjectionMatrix();
    glm::vec4 projInfo(
                       2.0f/proj[0].x,
                       2.0f/proj[1].y,
                       -(1.f-proj[0].z)/proj[0].x,
                       -(1.f+proj[1].z)/proj[1].y
                       );
    
    depth2RangesShader->Use();
    depth2RangesShader->SetUniform("clipInfo", glm::vec4(range.x*range.y, range.x-range.y, range.y, 1.f));
    depth2RangesShader->SetUniform("projInfo", projInfo);
    depth2RangesShader->SetUniform("rangeInfo", range);
    depth2RangesShader->SetUniform("texDepth", TEX_POSTPROCESS1);
    ((GraphicalSimulationApp*)SimulationApp::getApp())->getGLPipeline()->getContent()->DrawSAQ();
    glUseProgram(0);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLDepthCamera::DrawLDR(GLuint destinationFBO)
{
    //Check if there is a need to display image on screen
    bool display = true;
    if(camera != NULL)
        display = camera->getDisplayOnScreen();
    
    //Draw on screen
    if(display)
    {
        if(usesRanges) Depth2LinearRanges();
        else LinearizeDepth();
        
        //Bind depth texture
        glActiveTexture(GL_TEXTURE0 + TEX_POSTPROCESS1);
        glBindTexture(GL_TEXTURE_2D, linearDepthTex);
       
        //LDR drawing
        glBindFramebuffer(GL_FRAMEBUFFER, destinationFBO);
        glViewport(originX, originY, viewportWidth, viewportHeight);
        depthVisualizeShader->Use();
        depthVisualizeShader->SetUniform("texLinearDepth", TEX_POSTPROCESS1);
        depthVisualizeShader->SetUniform("range", range);
        ((GraphicalSimulationApp*)SimulationApp::getApp())->getGLPipeline()->getContent()->DrawSAQ();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(0);
        
        //Unbind textures
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    //Copy texture to camera buffer
    if(camera != NULL && update)
    {
        if(!display)
        {
            if(usesRanges) Depth2LinearRanges();
            else LinearizeDepth();
        }
        
        glBindTexture(GL_TEXTURE_2D, linearDepthTex);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, camera->getImageDataPointer(idx));
        glBindTexture(GL_TEXTURE_2D, 0);
         
        //Inform camera to run callback
        camera->NewDataReady(idx);
    }
    
    update = false;
}

///////////////////////// Static /////////////////////////////
void OpenGLDepthCamera::Init()
{
	depthLinearizeShader = new GLSLShader("depthLinearize.frag");
	depthLinearizeShader->AddUniform("clipInfo", ParameterType::VEC4);
	depthLinearizeShader->AddUniform("texDepth", ParameterType::INT);
    
    depth2RangesShader = new GLSLShader("depth2Ranges.frag");
    depth2RangesShader->AddUniform("clipInfo", ParameterType::VEC4);
    depth2RangesShader->AddUniform("projInfo", ParameterType::VEC4);
    depth2RangesShader->AddUniform("rangeInfo", ParameterType::VEC2);
    depth2RangesShader->AddUniform("texDepth", ParameterType::INT);
    
    depthVisualizeShader = new GLSLShader("depthVisualize.frag");
    depthVisualizeShader->AddUniform("range", ParameterType::VEC2);
    depthVisualizeShader->AddUniform("texLinearDepth", ParameterType::INT);
}

void OpenGLDepthCamera::Destroy()
{
    if(depthLinearizeShader != NULL) delete depthLinearizeShader;
    if(depth2RangesShader != NULL) delete depth2RangesShader;
    if(depthVisualizeShader != NULL) delete depthVisualizeShader;
}

}
