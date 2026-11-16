cmake_minimum_required(VERSION 3.20)
set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK version")
project(DX12PlaneGame CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.h" "src/*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${SOURCES} ${HEADERS})

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# DX12 çizimi, görüntü çözme ve ses için gereken Windows kütüphaneleri.
# Windows libraries for DX12 rendering, image decoding and audio.
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
    windowscodecs.lib
    ole32.lib
    xaudio2.lib
    gdi32.lib
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    $<$<CONFIG:Debug>:_DEBUG>
)

# FBX desteği isteğe bağlıdır; mevcut OBJ modeli ek bağımlılık gerektirmez.
# FBX support is optional; the supplied OBJ model needs no extra dependency.
find_package(assimp CONFIG QUIET)
if(assimp_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_ASSIMP)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_custom_target(CopyGameData ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Copying game assets"
)
