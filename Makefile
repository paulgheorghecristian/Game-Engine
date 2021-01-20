SHELL := /bin/bash

ifeq ($(origin MAKEFILE_DIR), undefined)
        MAKEFILE_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
endif

INCLUDES := \
-Ires/libraries/glm \
-Ires/libraries/glew/include \
-Ires/libraries/bullet \
-Ires/libraries/bullet/bullet \
-Ires/libraries/bullet/bullet/BulletCollision/CollisionDispatch \
-IRendering \
-IRendering/SkeletalAnimation \
-ICore \
-Ires/libraries/rapidjson/include \
-ICore/Utils \
-ICore/DataStructures \
-ICore/Loaders \
-IGame \
-IPhysics \
-IComponents \
-Ires/libraries/stb \
-Ires/libraries/imgui \
-I/usr/include/SDL2 \
-Ires/libraries/assimp/include

GAME_ENGINE_BINARY := ge

GAME_ENGINE_CC_SRCS := \
$(wildcard *.cpp) \
$(wildcard Core/DataStructures/*.cpp) \
$(wildcard Core/*.cpp) \
$(wildcard Core/Utils/*.cpp) \
$(wildcard Core/Loaders/*.cpp) \
$(wildcard Game/*.cpp) \
$(wildcard Physics/*.cpp) \
$(wildcard Rendering/*.cpp) \
$(wildcard Rendering/SkeletalAnimation/*.cpp) \
$(wildcard Components/*.cpp) \
$(wildcard res/libraries/imgui/*.cpp)

CC_PREFIX :=

CXX := $(CC_PREFIX)g++

LIBS := \
-lGL \
-lGLEW \
-lSDL2 \
-lSDL2_image \
-lSDL2_ttf \
-lBulletDynamics \
-lBulletCollision \
-lLinearMath \
-lassimp

LDOPTS := \
-Lres/libraries/bullet/lib \
-Wl,-rpath=res/libraries/bullet/lib \
-Lres/libraries/assimp/lib \
-Wl,-rpath=res/libraries/assimp/lib

#CXXFLAGS := -Wall -O3 -DNDEBUG
CXXFLAGS := -Wall -O0 -g

OBJDIR := $(MAKEFILE_DIR)/obj/

GE_OBJS := $(addprefix $(OBJDIR), \
$(patsubst %.cpp,%.o,$(GAME_ENGINE_CC_SRCS)))

$(OBJDIR)%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@


${GAME_ENGINE_BINARY}: $(GE_OBJS)
	$(CXX) $(CXXFLAGS) ${LDOPTS} $(INCLUDES) \
	-o ${GAME_ENGINE_BINARY} ${GE_OBJS} \
	$(LIBS)

all: ${GAME_ENGINE_BINARY}

clean:
	rm -rf $(MAKEFILE_DIR)/obj
	rm -rf ${GAME_ENGINE_BINARY}

