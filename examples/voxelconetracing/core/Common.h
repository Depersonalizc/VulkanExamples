#pragma once

#define FEATURE_OVR false
#define FEATURE_GLFW false

#include <iostream>
#include <vector>
#include <algorithm>
#include <array>

//#if FEATURE_GLFW
//#define GLFW_INCLUDE_VULKAN
//#include <GLFW/glfw3.h>
//#endif
#include <vulkanexamplebase.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/vec4.hpp>
#include <glm/glm/mat4x4.hpp>
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#include <fstream>

#include <chrono>

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

enum GBUFFER
{
	BASIC_COLOR = 0, SPECULAR_COLOR, NORMAL_COLOR, EMISSIVE_COLOR
};


#define NUM_GBUFFERS 4
#define NUM_DEBUGDISPLAY 12

#define DOWNSAMPLING_BLOOM 2.0f

#define WORKGROUP_X_SIZE_MAX 1024
#define WORKGROUP_Y_SIZE_MAX 1024
#define WORKGROUP_Z_SIZE_MAX 64

static bool bDeubDisply = false;
static bool bVRmode = false;
static bool bRotateMainLight = false;

static int autoCameraMove = -1;

static uint32_t drawMode = 4;

