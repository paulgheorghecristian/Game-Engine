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
-ICore \
-Ires/libraries/rapidjson/include \
-ICore/Utils \
-IGame \
-IPhysics \
-IComponents \
-Ires/libraries/stb \
-Ires/libraries/imgui \
-I/usr/include/SDL2

GAME_ENGINE_BINARY := ge

GAME_ENGINE_CC_SRCS := \
$(wildcard *.cpp) \
$(wildcard Core/*.cpp) \
$(wildcard Core/Utils/*.cpp) \
$(wildcard Game/*.cpp) \
$(wildcard Physics/*.cpp) \
$(wildcard Rendering/*.cpp) \
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
-lLinearMath

LDOPTS := \
-Lres/libraries/bullet/lib \
-Wl,-rpath=res/libraries/bullet/lib

CXXFLAGS := -Wall -O3 -DNDEBUG

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

