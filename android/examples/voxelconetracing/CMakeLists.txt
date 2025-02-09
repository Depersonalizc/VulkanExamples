cmake_minimum_required(VERSION 3.4.1 FATAL_ERROR)

set(NAME voxelconetracing)

set(SRC_DIR ../../../examples/${NAME})
set(BASE_DIR ../../../base)
set(EXTERNAL_DIR ../../../external)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -DVK_USE_PLATFORM_ANDROID_KHR -DVK_NO_PROTOTYPES")

file(GLOB EXAMPLE_SRC "${SRC_DIR}/*.cpp"
                      "${SRC_DIR}/actors/*.h"    "${SRC_DIR}/actors/*.cpp"
                      "${SRC_DIR}/assets/*.h"    "${SRC_DIR}/assets/*.cpp"    "${SRC_DIR}/assets/*.cc"
                      "${SRC_DIR}/core/*.h"      "${SRC_DIR}/core/*.cpp"
                      "${SRC_DIR}/external/stb-master/stb_image.h"
                      )

add_library(native-lib SHARED ${EXAMPLE_SRC})

add_library(native-app-glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

add_subdirectory(../base ${CMAKE_SOURCE_DIR}/../base)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

include_directories(${BASE_DIR}
                    ${EXTERNAL_DIR}
                    ${EXTERNAL_DIR}/glm
                    ${EXTERNAL_DIR}/imgui
                    ${EXTERNAL_DIR}/vk_video
                    ${EXTERNAL_DIR}/vulkan
                    ${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(
    native-lib
    native-app-glue
    libbase
    android
    log
    z
)
