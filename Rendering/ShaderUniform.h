#ifndef SHADERUNIFORM_H
#define SHADERUNIFORM_H

#include "glm/glm.hpp"
#include "Shader.h"
#include <string>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

class Shader;

enum ShaderUniformType {
    NO_TYPE = 0,
    UNIFORM_MAT4,
    UNIFORM_VEC2,
    UNIFORM_VEC3,
    UNIFORM_VEC4,
    UNIFORM_FLOAT,
    UNIFORM_INT,
    UNIFORM_BOOLEAN,
    UNIFORM_MAT4s,
    LAST_TYPE = UNIFORM_MAT4s + 1
};

union ShaderUniformValue {
    float mat4[16];
    struct vec4 {
        float x,y,z,w;
    } vec4;
    struct vec3 {
        float x,y,z;
    } vec3;
    float f;
    struct vec2 {
        float x,y;
    } vec2;
    int i;
    bool b;
};

class ShaderUniform
{
    public:
        ShaderUniform(const std::string& name, const char * type, Shader * shader);
        void updateUniform(void * data);
        void updateUniform(const std::vector<glm::mat4> &mats);
        ShaderUniformType getUniformType ();
        ShaderUniformValue getUniformValue ();
        void reload ();
        virtual ~ShaderUniform();

    protected:

    private:
        ShaderUniformType type;
        ShaderUniformValue value;
        Shader *shader;
        GLuint location;
        std::string name;

        static std::unordered_map<std::string, ShaderUniformType> uniformTypesStringMap;

        void updateVec2 (const glm::vec2 &data);
        void updateVec3 (const glm::vec3 &data);
        void updateVec4 (const glm::vec4 &data);
        void updateInt (int data);
        void updateFloat (float data);
        void updateBool (bool data);
        void updateMat4 (const glm::mat4 &data);
        ShaderUniformType getUniformTypeFromName(const char * name);

        static std::unordered_map<std::string, ShaderUniformType> getUniformTypesStringMap();
};

#endif // SHADERUNIFORM_H
