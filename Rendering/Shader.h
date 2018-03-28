#ifndef SHADER_H
#define SHADER_H

#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <GL/glew.h>
#include <string.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

#include "ShaderUniform.h"
#include "FileUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include "MemoryPoolInterface.h"

class ShaderUniform;

class Shader : public MemoryPoolInterface<Shader>
{
    public:
        Shader();
        Shader(const std::string &jsonPath);
        Shader *construct (const std::string &jsonPath);
        void reload();
        void bind();
        void unbind();
        GLuint getUniformLocation(const std::string &name);
        bool updateUniform (const std::string &name, void * data);
        bool updateUniform (const std::string &name, int data);
        virtual ~Shader();

    protected:

    private:
        GLuint vertexHandle, fragmentHandle, geometryHandle;
        GLuint program;
        std::unordered_map<std::string, ShaderUniform *> uniforms;
        const std::string shaderPath;

        const std::string loadShader(const std::string &filename);
};

#endif // SHADER_H
