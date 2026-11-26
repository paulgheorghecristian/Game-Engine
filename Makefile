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
-IRendering/DebugTools \
-IAudio \
-ICore \
-Ires/libraries/rapidjson/include \
-ICore/Utils \
-ICore/DataStructures \
-ICore/Loaders \
-IGame \
-IPhysics \
-IComponents \
-IGUIs \
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
$(wildcard Rendering/DebugTools/*.cpp) \
$(wildcard Audio/*.cpp) \
$(wildcard Components/*.cpp) \
$(wildcard GUIs/*.cpp) \
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

# System deps: sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libglew-dev libglu1-mesa-dev libgl-dev
# Bullet and Assimp are vendored; rpaths are $$ORIGIN-relative so the binary
# works regardless of the current working directory.
LDOPTS := \
-Lres/libraries/bullet/lib \
-Wl,-rpath,'$$ORIGIN/res/libraries/bullet/lib' \
-Lres/libraries/assimp/lib \
-Wl,-rpath,'$$ORIGIN/res/libraries/assimp/lib'

# `make DEBUG=1` for an unoptimized debug build.
ifeq ($(DEBUG),1)
CXXFLAGS := -std=gnu++20 -Wall -O0 -g -MMD -MP -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL
OBJDIR := $(MAKEFILE_DIR)/obj/debug/
else
CXXFLAGS := -std=gnu++20 -Wall -O2 -g -DNDEBUG -MMD -MP -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL
OBJDIR := $(MAKEFILE_DIR)/obj/release/
endif

GE_OBJS := $(addprefix $(OBJDIR), \
$(patsubst %.cpp,%.o,$(GAME_ENGINE_CC_SRCS)))

$(OBJDIR)%.o: %.cpp Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@


${GAME_ENGINE_BINARY}: $(GE_OBJS)
	$(CXX) $(CXXFLAGS) ${LDOPTS} $(INCLUDES) \
	-o ${GAME_ENGINE_BINARY} ${GE_OBJS} \
	$(LIBS)

all: ${GAME_ENGINE_BINARY}

-include $(GE_OBJS:.o=.d)

.PHONY: all clean

clean:
	rm -rf $(MAKEFILE_DIR)/obj
	rm -rf ${GAME_ENGINE_BINARY}

