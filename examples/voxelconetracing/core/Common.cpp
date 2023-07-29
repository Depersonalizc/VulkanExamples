#include "Common.h"


const std::string getShaderPath(std::string shaderfile)
{
    return getShaderBasePath() + "glsl/voxelconetracing/" + shaderfile;
}

const std::string getModelPath(std::string modelfile)
{
    return getAssetPath() + "models/vct/" + modelfile;
}

const std::string getTexturePath(std::string texturefile)
{
    return getAssetPath() + "textures/vct/" + texturefile;
}

std::vector<char> readFile(const std::string filename)
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


#if defined(__ANDROID__)

#define MAX_DIRLEN 100

static int android_read(void *cookie, char *buf, int size);
static int android_write(void *cookie, const char *buf, int size);
static fpos_t android_seek(void *cookie, fpos_t offset, int whence);
static int android_close(void *cookie);


/*
static AAssetManager* assetManager = nullptr;          // Android assets manager pointer
static const char* internalDataPath = nullptr;         // Android internal data path


// Initialize asset manager from android app
void InitAssetManager(AAssetManager* manager, const char* dataPath)
{
	assetManager = manager;
	internalDataPath = dataPath;
}
*/

// Replacement for fopen()
// Ref: https://developer.android.com/ndk/reference/group/asset
FILE* android_fopen(const char* fileName, const char* mode)
{
    if (mode[0] == 'w')
    {
        // fopen() is mapped to android_fopen() that only grants read access to
        // assets directory through AAssetManager but we want to also be able to
        // write data when required using the standard stdio FILE access functions
        // Ref: https://stackoverflow.com/questions/11294487/android-writing-saving-files-from-native-code-only
#undef fopen
        char name[MAX_DIRLEN];
        snprintf(name, MAX_DIRLEN, "%s/%s", androidApp->activity->internalDataPath, fileName);
        return fopen(name, mode);
#define fopen(name, mode) android_fopen(name, mode)
    }
    else
    {
        // NOTE: AAsset provides access to read-only asset
        AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, fileName, AASSET_MODE_STREAMING);

        if (asset != NULL)
        {
            // Get pointer to file in the assets
            return funopen(asset, android_read, android_write, android_seek, android_close);
        }
        else
        {
#undef fopen
            // Just do a regular open if file is not found in the assets
            char name[MAX_DIRLEN];
            snprintf(name, MAX_DIRLEN, "%s/%s", androidApp->activity->internalDataPath, fileName);
            return fopen(name, mode);
#define fopen(name, mode) android_fopen(name, mode)
        }
    }
}

static int android_read(void *cookie, char *buf, int size)
{
    return AAsset_read((AAsset *)cookie, buf, size);
}

static int android_write(void *cookie, const char *buf, int size)
{
    fprintf(stderr, "ANDROID: Failed to provide write access to APK");
    return EACCES;
}

static fpos_t android_seek(void *cookie, fpos_t offset, int whence)
{
    return AAsset_seek((AAsset *)cookie, offset, whence);
}

static int android_close(void *cookie)
{
    AAsset_close((AAsset *)cookie);
    return 0;
}

#endif  // __ANDROID__