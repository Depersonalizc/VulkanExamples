#include "Common.h"

#if defined(__ANDROID__)

#define MAX_DIRLEN 100

static AAssetManager* assetManager = nullptr;          // Android assets manager pointer
static const char* internalDataPath = nullptr;         // Android internal data path

// Initialize asset manager from android app
void InitAssetManager(AAssetManager* manager, const char* dataPath)
{
	assetManager = manager;
	internalDataPath = dataPath;
}

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
        snprintf(name, MAX_DIRLEN, "%s/%s", internalDataPath, fileName);
        return fopen(name, mode);
#define fopen(name, mode) android_fopen(name, mode)
    }
    else
    {
        // NOTE: AAsset provides access to read-only asset
        AAsset* asset = AAssetManager_open(assetManager, fileName, AASSET_MODE_UNKNOWN);

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
            snprintf(name, MAX_DIRLEN, "%s/%s", internalDataPath, fileName);
            return fopen(name, mode);
#define fopen(name, mode) android_fopen(name, mode)
        }
    }
}

#endif  // __ANDROID__