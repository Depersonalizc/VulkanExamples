#pragma once

#if defined(__ANDROID__)
#include <VulkanAndroid.h>
#include <stdio.h>                      // Required for: FILE, snprintf, ...
#define fopen(name, mode) android_fopen(name, mode)
//void InitAssetManager(AAssetManager* manager, const char* dataPath);   // Initialize asset manager from android app
FILE* android_fopen(const char* fileName, const char* mode);           // Replacement for fopen() -> Read-only!
#endif



#include <iostream>
#include <vector>
#include <algorithm>
#include <array>

#include <VulkanTools.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/vec4.hpp>
#include <glm/glm/mat4x4.hpp>
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#include <fstream>

#include <chrono>

const std::string getShaderPath(std::string shaderfile);
const std::string getModelPath(std::string modelfile);
const std::string getTexturePath(std::string texturefile);
std::vector<char> readFile(const std::string filename);

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

