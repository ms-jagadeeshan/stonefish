//
//  OpenGLSun.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 20/04/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#include "OpenGLSun.h"
#include "OpenGLSky.h"
#include "OpenGLLight.h"
#include "SimulationApp.h"
#include "Console.h"

OpenGLSun* OpenGLSun::instance = NULL;

OpenGLSun* OpenGLSun::getInstance()
{
    if(instance == NULL)
        instance = new OpenGLSun();
    
    return instance;
}

OpenGLSun::OpenGLSun()
{
    shadowmapShader = NULL;
    shadowTextureUnit = 0;
    sunElevation = 0.f;
    sunAzimuth = 0.f;
    shadowmapArray = 0;
    shadowmapSplits = 4;
    shadowmapSize = 4096;
    shadowFBO = 0;
    sunDirection = btVector3(0.,0.,1.);
    sunColor = glm::vec4(1.0,1.0,1.0,1.0);
    sunModelview = glm::mat4x4(0);
    frustum = NULL;
    shadowCPM = NULL;
}

OpenGLSun::~OpenGLSun()
{
    glDeleteTextures(1, &shadowmapArray);
    glDeleteFramebuffers(1, &shadowFBO);
    delete shadowmapShader;
}

void OpenGLSun::Init()
{
    //Load shader
    shadowmapShader = new GLSLShader("cascadedShadowMap.frag");
    shadowmapShader->AddUniform("shadowmapArray", INT);
    shadowmapShader->AddUniform("shadowmapLayer", FLOAT);
    
    //Create shadowmap texture array
    glGenTextures(1, &shadowmapArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, shadowmapArray);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, shadowmapSize, shadowmapSize, shadowmapSplits, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    
    //Create shadowmap framebuffer
    glGenFramebuffers(1, &shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowmapArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
        cError("Sun shadow FBO initialization failed!");
    
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    frustum = new ViewFrustum[shadowmapSplits];
    shadowCPM = new glm::mat4x4[shadowmapSplits];
}

glm::vec3 OpenGLSun::GetSunDirection()
{
    return glm::vec3(sunDirection.getX(), sunDirection.getY(), sunDirection.getZ());
}

glm::vec4 OpenGLSun::GetSunColor()
{
    return sunColor;
}

//Computes the near and far distances for every frustum slice in camera eye space
void OpenGLSun::UpdateSplitDist(GLfloat nd, GLfloat fd)
{
	GLfloat lambda = 0.8f;
	GLfloat ratio = fd/nd;
	frustum[0].near = nd;
    
	for(int i=1; i < shadowmapSplits; i++)
	{
		GLfloat si = i / (GLfloat)shadowmapSplits;
        
		frustum[i].near = lambda*(nd*powf(ratio, si)) + (1.f-lambda)*(nd + (fd - nd)*si);
		frustum[i-1].far = frustum[i].near * 1.005f;
	}
    
	frustum[shadowmapSplits-1].far = fd;
    
    //for(int i=0; i<shadowmapSplits; i++) printf("Frustum%d Near: %f Far: %f\n", i, frustum[i].near, frustum[i].far);
}

//Computes the 8 corner points of the current view frustum
void OpenGLSun::UpdateFrustumCorners(ViewFrustum &f, glm::vec3 center, glm::vec3 dir, glm::vec3 up)
{
    glm::vec3 right = glm::cross(dir, up);
    right = glm::normalize(right);
	//up = glm::normalize(glm::cross(right, dir));
    
    glm::vec3 fc = center + dir * f.far;
    glm::vec3 nc = center + dir * f.near;
    
	// these heights and widths are half the heights and widths of
	// the near and far plane rectangles
	GLfloat nearHeight = tan(f.fov/2.0f) * f.near;
	GLfloat nearWidth = nearHeight * f.ratio;
	GLfloat farHeight = tan(f.fov/2.0f) * f.far;
	GLfloat farWidth = farHeight * f.ratio;
    
	f.corners[0] = nc - up * nearHeight - right * nearWidth;
	f.corners[1] = nc + up * nearHeight - right * nearWidth;
	f.corners[2] = nc + up * nearHeight + right * nearWidth;
	f.corners[3] = nc - up * nearHeight + right * nearWidth;
	f.corners[4] = fc - up * farHeight - right * farWidth;
	f.corners[5] = fc + up * farHeight - right * farWidth;
	f.corners[6] = fc + up * farHeight + right * farWidth;
	f.corners[7] = fc - up * farHeight + right * farWidth;
}

// this function builds a projection matrix for rendering from the shadow's POV.
// First, it computes the appropriate z-range and sets an orthogonal projection.
// Then, it translates and scales it, so that it exactly captures the bounding box
// of the current frustum slice
glm::mat4 OpenGLSun::BuildCropProjMatrix(ViewFrustum &f)
{
	GLfloat maxX = -1000.0f;
    GLfloat maxY = -1000.0f;
	GLfloat maxZ;
    GLfloat minX =  1000.0f;
    GLfloat minY =  1000.0f;
	GLfloat minZ;
    glm::vec4 transf;
	
	//Find the z-range of the current frustum as seen from the light in order to increase precision
	glm::mat4 shad_mv = sunModelview;
    
	//Note: only the z-component is needed and thus the multiplication can be simplified
	//transf.z = shad_modelview[2] * f.point[0].x + shad_modelview[6] * f.point[0].y + shad_modelview[10] * f.point[0].z + shad_modelview[14]
	transf = shad_mv * glm::vec4(f.corners[0], 1.0f);
	minZ = transf.z;
	maxZ = transf.z;
    
	for(int i = 1; i <8 ; i++)
	{
		transf = shad_mv * glm::vec4(f.corners[i], 1.0f);
		if(transf.z > maxZ) maxZ = transf.z;
		if(transf.z < minZ) minZ = transf.z;
	}
    
	//Make sure all relevant shadow casters are included - use object bounding boxes!
    btVector3 aabbMin;
    btVector3 aabbMax;
    SimulationApp::getApp()->getSimulationManager()->getWorldAABB(aabbMin, aabbMax);
    
    transf = shad_mv * glm::vec4(aabbMin.x(), aabbMin.y(), aabbMin.z(), 1.f);
    if(transf.z > maxZ) maxZ = transf.z;
    if(transf.z < minZ) minZ = transf.z;
    
    transf = shad_mv * glm::vec4(aabbMax.x(), aabbMax.y(), aabbMax.z(), 1.f);
    if(transf.z > maxZ) maxZ = transf.z;
    if(transf.z < minZ) minZ = transf.z;
    
	//Set the projection matrix with the new z-bounds
	//Note: there is inversion because the light looks at the neg z axis
    glm::mat4 shad_proj = glm::ortho(-1.f, 1.f, -1.f, 1.f, -maxZ, -minZ);
	glm::mat4 shad_mvp =  shad_proj * shad_mv;
    
	//Find the extends of the frustum slice as projected in light's homogeneous coordinates
    for(int i = 0; i < 8; i++)
	{
		transf = shad_mvp * glm::vec4(f.corners[i], 1.0f);

		transf.x /= transf.w;
		transf.y /= transf.w;
        
		if(transf.x > maxX) maxX = transf.x;
		if(transf.x < minX) minX = transf.x;
		if(transf.y > maxY) maxY = transf.y;
		if(transf.y < minY) minY = transf.y;
	}
    
    //Build crop matrix
    glm::mat4 shad_crop = glm::mat4(1.f);
	GLfloat scaleX = 2.0f/(maxX - minX);
	GLfloat scaleY = 2.0f/(maxY - minY);
	GLfloat offsetX = -0.5f*(maxX + minX)*scaleX;
	GLfloat offsetY = -0.5f*(maxY + minY)*scaleY;
    shad_crop[0][0] = scaleX;
    shad_crop[1][1] = scaleY;
    shad_crop[3][0] = offsetX;
    shad_crop[3][1] = offsetY;
    
    //Build crop projection matrix
    glm::mat4 shad_crop_proj = shad_crop * shad_proj;
    
	return shad_crop_proj;
}

void OpenGLSun::SetupShader(GLSLShader* shader)
{	
	//Calculate sun color
    sunColor = OpenGLLight::ColorFromTemperature(5000.f + 20.f*sunElevation, SUN_SKY_FACTOR * sin(sunElevation / 180.0 * M_PI));
    
	//Calculate shadow splits
	glm::mat4 bias(0.5f, 0.f, 0.f, 0.f,
                   0.f, 0.5f, 0.f, 0.f,
                   0.f, 0.f, 0.5f, 0.f,
                   0.5f, 0.5f, 0.5f, 1.f);
    glm::vec4 frustumFar;
    glm::mat4 lightClipSpace[4];
    
	//For every inactive split
    for(int i=shadowmapSplits; i < 4; ++i)
    {
		frustumFar[i] = 0;
        lightClipSpace[i] = glm::mat4();
    }
    
	//For every active split
	for(int i=0; i<shadowmapSplits; ++i)
	{
		frustumFar[i] = frustum[i].far;
		lightClipSpace[i] = (bias * shadowCPM[i]); // compute a matrix that transforms from world space to light clip space
	}
	
	shader->SetUniform("sunDirection", GetSunDirection());
	shader->SetUniform("sunColor", sunColor);
	shader->SetUniform("sunFrustumFar", frustumFar);
	shader->SetUniform("sunClipSpace[0]", lightClipSpace[0]);
	shader->SetUniform("sunClipSpace[1]", lightClipSpace[1]);
	shader->SetUniform("sunClipSpace[2]", lightClipSpace[2]);
	shader->SetUniform("sunClipSpace[3]", lightClipSpace[3]);
	shader->SetUniform("sunShadowMap", TEX_SUN_SHADOW);

	glActiveTexture(GL_TEXTURE0 + TEX_SUN_SHADOW);
    glEnable(GL_TEXTURE_2D_ARRAY);
	glBindTexture(GL_TEXTURE_2D_ARRAY, shadowmapArray);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
}

void OpenGLSun::RenderShadowMaps(OpenGLPipeline* pipe, OpenGLView* view, SimulationManager* sim)
{
    //Pre-set splits
    for(int i = 0; i < shadowmapSplits; ++i)
    {
        frustum[i].fov = view->GetFOVY() + 0.2f; //avoid artifacts in the borders
        GLint* viewport = view->GetViewport();
        frustum[i].ratio = (GLfloat)viewport[2]/(GLfloat)viewport[3];
        delete [] viewport;
    }
    
    //Compute the z-distances for each split as seen in camera space
    UpdateSplitDist(view->GetNearClip(), view->GetFarClip());
    
    //Render maps
    glCullFace(GL_FRONT); //GL_FRONT -> no shadow acne but problems with filtering
    
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glViewport(0, 0, shadowmapSize, shadowmapSize);
    
    glm::vec3 camPos = view->GetEyePosition();
    glm::vec3 camDir = view->GetLookingDirection();
    glm::vec3 camUp = view->GetUpDirection();
    
	// for all shadow splits
	for(int i = 0; i < shadowmapSplits; i++)
	{
		//Compute the camera frustum slice boundary points in world space
        UpdateFrustumCorners(frustum[i], camPos, camDir, camUp);
		
        //Adjust the view frustum of the light, so that it encloses the camera frustum slice fully.
		//note that this function sets the projection matrix as it sees best fit		
        glm::mat4 cp = BuildCropProjMatrix(frustum[i]);
        shadowCPM[i] =  cp * sunModelview;

		OpenGLContent::getInstance()->SetProjectionMatrix(cp);
		OpenGLContent::getInstance()->SetViewMatrix(sunModelview);
        //Draw current depth map
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowmapArray, 0, i);
		glClear(GL_DEPTH_BUFFER_BIT);
        pipe->DrawObjects(sim);
	}
    
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glCullFace(GL_BACK);
}

void OpenGLSun::ShowFrustumSplits()
{
    std::vector<glm::vec3> vertices;
	
	for(unsigned int i = 0; i < shadowmapSplits; ++i)
    {
    	vertices.push_back(frustum[i].corners[4]);
		vertices.push_back(frustum[i].corners[5]);
		vertices.push_back(frustum[i].corners[6]);
		vertices.push_back(frustum[i].corners[7]);
		vertices.push_back(frustum[i].corners[4]);
        
		OpenGLContent::getInstance()->DrawPrimitives(PrimitiveType::LINE_STRIP, vertices, DUMMY_COLOR);
		vertices.clear();
	}
}

void OpenGLSun::ShowShadowMaps(GLfloat x, GLfloat y, GLfloat scale)
{
    //Texture setup
	glActiveTexture(GL_TEXTURE0 + TEX_SUN_SHADOW);
    glBindTexture(GL_TEXTURE_2D_ARRAY, shadowmapArray);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    
	//Render the shadowmaps
    shadowmapShader->Use();
    shadowmapShader->SetUniform("shadowmapArray", TEX_SUN_SHADOW);
    for(int i = 0; i < shadowmapSplits; i++)
    {
        glViewport(x + shadowmapSize * scale * i, y, shadowmapSize * scale, shadowmapSize * scale);
        shadowmapShader->SetUniform("shadowmapLayer", (GLfloat)i);
        OpenGLContent::getInstance()->DrawSAQ();
    }
    glUseProgram(0);
    
	//Reset
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void OpenGLSun::SetPosition(GLfloat elevation, GLfloat azimuth)
{
    sunElevation = elevation;
    sunAzimuth = azimuth;
    
    //calculate sun direction
    bool zUp = SimulationApp::getApp()->getSimulationManager()->isZAxisUp();

    sunDirection = btVector3(0.f, -1.f, 0.f);
    if(zUp)
    {
        sunDirection = sunDirection.rotate(btVector3(1.f,0.f,0.f), glm::radians(180.f - sunElevation));
        sunDirection = sunDirection.rotate(btVector3(0.f,0.f,1.f), glm::radians(90.f - sunAzimuth));
    }
    else
    {
        sunDirection = sunDirection.rotate(btVector3(1.f,0.f,0.f), glm::radians(180.f + sunElevation));
        sunDirection = sunDirection.rotate(btVector3(0.f,0.f,1.f), glm::radians(90.f + sunAzimuth));
    }
    sunDirection.normalize();
    
    //build sun modelview
    glm::vec3 dir(sunDirection.x(), sunDirection.y(), sunDirection.z());
    glm::vec3 up(0.f, 0.f, zUp ? 1.f : -1.f);
    glm::vec3 right = glm::cross(dir, up);
	right = glm::normalize(right);
	up = glm::normalize(glm::cross(right, dir));
    sunModelview = glm::lookAt(glm::vec3(0,0,0), dir, up) * glm::mat4(1.f);
}