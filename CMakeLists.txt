cmake_minimum_required(VERSION 2.8)

project(SDL2UniSampler)

set(CMAKE_CXX_FLAGS "-std=c++14 -Wall")

# SDL2
list(APPEND CMAKE_MODULE_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/FindCMakeModules/"
)
find_package(SDL2)

# Python libraries for pyliaison
if (WIN32)
	set(SDL2_LIBS ${SDL2_LIBRARY} ${SDLMAIN_LIBRARY})
else(WIN32)
	set(SDL2_LIBS ${SDL2_LIBRARY})
endif(WIN32)

# Add subdir for UniSampler
ADD_SUBDIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/UniSampler)

# Source files, include files, scripts
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# Create source groups
source_group("Source" FILES ${SOURCES})
source_group("Include" FILES ${HEADERS})

# Add the SDL2UniSampler executable
add_executable(SDL2UniSampler ${SOURCES} ${HEADERS})

# Make sure it gets its include paths
target_include_directories(SDL2UniSampler PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${SDL2_INCLUDE_DIR} 
    ${UNISAMPLER_INC}
)

target_link_libraries(SDL2UniSampler LINK_PUBLIC 
    ${SDL2_LIBS} 
    tinyxml2
    UniSampler
)