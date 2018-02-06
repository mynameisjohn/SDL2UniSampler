cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/FindCMakeModules/"
)
include(BuildType)
project(SDL2UniSampler)

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

IF(${CMAKE_BUILD_TYPE} STREQUAL "DEBUG")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -G")
ENDIF(${CMAKE_BUILD_TYPE} STREQUAL "DEBUG")

# let CMake find CUDA
FIND_PACKAGE(CUDA QUIET REQUIRED)
# SET(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS}; -arch sm_52 -Xcompiler -rdynamic -lineinfo")

# SDL2
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
cuda_add_executable(SDL2UniSampler ${SOURCES} ${HEADERS})

# Make sure it gets its include paths
target_include_directories(SDL2UniSampler PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${SDL2_INCLUDE_DIR} 
    ${UNISAMPLER_INC}
)

if (WIN32)
	SET(_WIN_XIPH_LIBS 
		"$ENV{SDK}/libsndfile/lib/x64/${CMAKE_BUILD_TYPE}/libsndfile.lib"
		"$ENV{SDK}/XIPH/vorbis/lib/x64/${CMAKE_BUILD_TYPE}/libvorbis_static.lib"
		"$ENV{SDK}/XIPH/FLAC/lib/x64/${CMAKE_BUILD_TYPE}/libFLAC_static.lib"
		"$ENV{SDK}/XIPH/ogg/lib/x64/${CMAKE_BUILD_TYPE}/libogg_static.lib")
	
	target_link_libraries(SDL2UniSampler LINK_PUBLIC 
		${SDL2_LIBS} 
		${_WIN_XIPH_LIBS}
		${CUDA_LIBRARIES}
		tinyxml2
		UniSampler
	)
ELSE (WIN32)
	target_link_libraries(SDL2UniSampler LINK_PUBLIC 
		${SDL2_LIBS} 
		tinyxml2
		UniSampler
		${CUDA_LIBRARIES}
		-lsndfile
	)
endif(WIN32)
