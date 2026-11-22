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
#include <vector>
#include <string_view>

#include "ShaderUniform.h"
#include "FileUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include "MemoryPoolInterface.h"

class ShaderUniform;

/* Transparent hash so uniforms can be looked up by string literal / string_view
 * without constructing (and heap-allocating) a std::string per call. */
struct ShaderStringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
};

class Shader : public MemoryPoolInterface<Shader>
{
    public:
        Shader();
        Shader(const std::string &jsonPath);
        Shader *construct(const std::string &jsonPath);
        void reload();
        void bind();
        void unbind();
        GLuint getUniformLocation(const std::string &name);
        bool updateUniform(std::string_view name, void * data);
        bool updateUniform(std::string_view name, int data);
        bool updateUniform(std::string_view name, float data);
        bool updateUniform(std::string_view name, bool data);
        bool updateUniform(std::string_view name, const std::vector<glm::mat4> &mats);

        /* Program currently bound through Shader::bind (avoids redundant glUseProgram). */
        static GLuint currentProgram;
        /* glProgramUniform* is available (GL 4.1 / ARB_separate_shader_objects):
         * uniforms can be set without binding the program. */
        static bool hasDSAUniforms();

        inline const std::string &getShaderPath() { return shaderPath; }
        inline GLuint getShaderId() { return program; }

        virtual ~Shader();

    protected:

    private:
        std::string shaderPath;
        GLuint vertexHandle, fragmentHandle, geometryHandle;
        GLuint program;
        std::unordered_map<std::string, ShaderUniform *, ShaderStringHash, std::equal_to<>> uniforms;
        ShaderUniform *findUniform(std::string_view name);

        const std::string loadShader(const std::string &filename);
};

#endif // SHADER_H
