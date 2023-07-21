#pragma once

#if FEATURE_OVR
#include <OVR_CAPI_Vk.h>
#include <Extras/OVR_Math.h>
#endif
#include "../core/Common.h"
#include "Actor.h"

class CameraActor : public Actor
{
public:
	CameraActor();
	~CameraActor();

	void setCamera(glm::vec3 eyePositionParam, glm::vec3 lookVectorParam, glm::vec3 upVectorParam, float fovYParam, float width, float height, float nearParam, float farParam);
	
#if FEATURE_OVR
	void setIPD(float param)
	{
		IPD = param;
	}
#endif

	void UpdateAspectRatio(float aspectRatio);
	
	virtual void UpdateOrbit(float deltaX, float deltaY, float deltaZ);
	virtual void UpdatePosition(float deltaX, float deltaY, float deltaZ);

#if FEATURE_OVR
	void setHmdState(const bool bRenderToHmd, ovrFovPort* fovport);
	void UpdateOrbitHmdVRSampleCode(const ovrPosef* eyeRenderPoset);
	void UpdateMatricesHmdVR(const glm::vec3& relativeLeftPos, const glm::vec3& relativeRightPos);
	void UpdateOrbitHmdVR(const glm::vec3& deltaHmdEuler, const glm::vec3& deltaHmdPos, const glm::vec3& relativeLeftPos, const glm::vec3& relativeRightPos);
#endif

	glm::vec3 centerPosition;
	glm::vec3 lookVector;
	
	glm::mat4 viewMat;
	glm::mat4 projMat;

	glm::mat4 viewProjMat;
	glm::mat4 InvViewProjMat;

	float nearPlane;
	float farPlane;
	float fovY;
	float aspectRatio;

	bool bRenderToHmd = false;
	bool vrMode;

#if FEATURE_OVR
	ovrFovPort g_EyeFov[2];

	std::vector<glm::mat4> viewMatforVR;
	std::vector<glm::mat4> projMatforVR;

	std::vector<glm::mat4> viewProjMatforVR;
	std::vector<glm::mat4> InvViewProjMatforVR;

	std::vector<glm::vec3> positionforVR;

	float omega = 0.f;//OCULUS SDK// Needed for HMD head roll
	float IPD;
	float focalDistance;
#endif

private:

	
};

