#include "ShaderUniform.h"

ShaderUniform::ShaderUniform(const std::string &name, const char * t, Shader * shader) : type (getUniformTypeFromName (t)),
                                                                                            shader (shader),
                                                                                            name (name) {
    shader->bind ();
    location = shader->getUniformLocation (name);
    shader->unbind ();
}

std::unordered_map<std::string, ShaderUniformType> ShaderUniform::getUniformTypesStringMap() {
    std::unordered_map<std::string, ShaderUniformType> typesMap;

    typesMap["MAT4"] = UNIFORM_MAT4;
    typesMap["VEC2"] = UNIFORM_VEC2;
    typesMap["VEC3"] = UNIFORM_VEC3;
    typesMap["VEC4"] = UNIFORM_VEC4;
    typesMap["FLOAT"] = UNIFORM_FLOAT;
    typesMap["INT"] = UNIFORM_INT;
    typesMap["BOOL"] = UNIFORM_BOOLEAN;

    return typesMap;
}

std::unordered_map<std::string, ShaderUniformType> ShaderUniform::uniformTypesStringMap = ShaderUniform::getUniformTypesStringMap ();

void ShaderUniform::updateUniform (void * data) {
    assert (type < LAST_TYPE);

    shader->bind ();

    switch (type) {
    case UNIFORM_MAT4:
        updateMat4 (*(glm::mat4 *)data);
        break;
    case UNIFORM_VEC2:
        updateVec2 (*(glm::vec2 *)data);
        break;
    case UNIFORM_VEC3:
        updateVec3 (*(glm::vec3 *)data);
        break;
    case UNIFORM_VEC4:
        updateVec4 (*(glm::vec4 *)data);
        break;
    case UNIFORM_INT:
        updateInt (*(int *)data);
    case UNIFORM_FLOAT:
        updateFloat (*(float *)data);
        break;
    case UNIFORM_BOOLEAN:
        updateBool (*(bool *)data);
        break;
    case NO_TYPE:
    case LAST_TYPE:
        break;
    }

    shader->unbind ();
}

void ShaderUniform::updateMat4 (const glm::mat4 &data) {
    glUniformMatrix4fv (location, 1, false, glm::value_ptr (data));
}

void ShaderUniform::updateVec2 (const glm::vec2 &data) {
    glUniform2f (location, data.x, data.y);
}

void ShaderUniform::updateVec3 (const glm::vec3 &data) {
    glUniform3f (location, data.x, data.y, data.z);
}

void ShaderUniform::updateVec4 (const glm::vec4 &data) {
    glUniform4f (location, data.x, data.y, data.z, data.w);
}

void ShaderUniform::updateInt (const int &data) {
    glUniform1i (location, data);
}

void ShaderUniform::updateFloat (const float &data) {
    glUniform1f (location, data);
}

void ShaderUniform::updateBool (const bool &data) {
    glUniform1i (location, (int) data);
}

ShaderUniformType ShaderUniform::getUniformTypeFromName(const char * name) {
    return uniformTypesStringMap[name];
}

ShaderUniformType ShaderUniform::getUniformType () {
    return type;
}

ShaderUniform::~ShaderUniform() {

}
