cmake_minimum_required(VERSION 3.24)
project(SkyRewrite VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep in sync with the CommonLibSSE-NG branch used by the SkyOS source tree.
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG ng
    GIT_SUBMODULES ""
    GIT_SHALLOW TRUE
)

set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE-NG tests" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable CommonLibSSE-NG tests" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "SkyRewrite supports Skyrim SE/AE, not VR" FORCE)

# CommonLib is a build dependency; do not copy its install rules into the mod package.
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(CommonLibSSE)
set(CMAKE_SKIP_INSTALL_RULES OFF)
include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

add_commonlibsse_plugin(SkyRewrite
    NAME "SkyRewrite"
    AUTHOR "SkyOS"
    VERSION ${PROJECT_VERSION}
    SOURCES
        src/skyrewrite.cpp
        src/app_module_entry.cpp
)

target_include_directories(SkyRewrite PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/data/SkyRewriteAPIHeader/include"
)
target_compile_features(SkyRewrite PRIVATE cxx_std_23)
target_compile_definitions(SkyRewrite PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    SKYOS_SKYREWRITE_EXPORTS
)
target_precompile_headers(SkyRewrite PRIVATE src/pch.h)

install(TARGETS SkyRewrite RUNTIME DESTINATION "Data/SKSE/Plugins")
install(DIRECTORY data/PrismaUI/views/SkyRewrite
    DESTINATION "Data/PrismaUI/views")
install(DIRECTORY data/SKSE/Plugins/SkyOS/Mods/SkyRewrite
    DESTINATION "Data/SKSE/Plugins/SkyOS/Mods")
install(DIRECTORY data/SKSE/Plugins/SkyOS/Mods/SkyViewer/SkyRewrite
    DESTINATION "Data/SKSE/Plugins/SkyOS/Mods/SkyViewer")

# The public API header and branded guide ship with this standalone project.
install(FILES data/SkyRewriteAPIHeader/include/SkyRewriteAPI.h
    DESTINATION "Data/SKSE/Plugins/SkyRewriteAPIHeader/include")
install(FILES data/SkyRewriteAPIHeader/README.md
    DESTINATION "Data/SKSE/Plugins/SkyRewriteAPIHeader")
install(DIRECTORY data/SKSE/Plugins/SkyOS/Mods/SkyRewriteAPIHeader
    DESTINATION "Data/SKSE/Plugins/SkyOS/Mods")

set(CPACK_PACKAGE_NAME "SkyRewrite")
set(CPACK_PACKAGE_VENDOR "SkyOS")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "SkyRewrite-${PROJECT_VERSION}-SkyrimSE-AE")
set(CPACK_GENERATOR "ZIP")
include(CPack)
