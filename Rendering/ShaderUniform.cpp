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
        break;
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
    assert (type == UNIFORM_MAT4);

    memcpy (value.mat4, (void *) &data, sizeof(glm::mat4));
    glUniformMatrix4fv (location, 1, false, glm::value_ptr (data));
}

void ShaderUniform::updateVec2 (const glm::vec2 &data) {
    assert (type == UNIFORM_VEC2);

    value.vec2.x = data.x;
    value.vec2.y = data.y;
    glUniform2f (location, data.x, data.y);
}

void ShaderUniform::updateVec3 (const glm::vec3 &data) {
    assert (type == UNIFORM_VEC3);

    value.vec3.x = data.x;
    value.vec3.y = data.y;
    value.vec3.z = data.z;
    glUniform3f (location, data.x, data.y, data.z);
}

void ShaderUniform::updateVec4 (const glm::vec4 &data) {
    assert (type == UNIFORM_VEC4);

    value.vec4.x = data.x;
    value.vec4.y = data.y;
    value.vec4.z = data.z;
    value.vec4.w = data.w;
    glUniform4f (location, data.x, data.y, data.z, data.w);
}

void ShaderUniform::updateInt (int data) {
    assert (type == UNIFORM_INT);

    value.i = data;
    glUniform1i (location, data);
}

void ShaderUniform::updateFloat (float data) {
    assert (type == UNIFORM_FLOAT);

    value.f = data;
    glUniform1f (location, data);
}

void ShaderUniform::updateBool (bool data) {
    assert (type == UNIFORM_BOOLEAN);

    value.b = data;
    glUniform1i (location, (int) data);
}

void ShaderUniform::reload () {
    assert (shader != NULL);

    shader->bind();

    switch (type) {
    case UNIFORM_MAT4: {
        glm::mat4 tmp;
        memcpy ((void *) &tmp, value.mat4, sizeof (glm::mat4));
        updateMat4 (tmp);
        break;
    }
    case UNIFORM_VEC2: {
        glm::vec3 tmp;
        tmp.x = value.vec2.x;
        tmp.y = value.vec2.y;
        updateVec2 (tmp);
        break;
    }
    case UNIFORM_VEC3: {
        glm::vec3 tmp;
        tmp.x = value.vec3.x;
        tmp.y = value.vec3.y;
        tmp.z = value.vec3.z;
        updateVec3 (tmp);
        break;
    }
    case UNIFORM_VEC4: {
        glm::vec4 tmp;
        tmp.x = value.vec4.x;
        tmp.y = value.vec4.y;
        tmp.z = value.vec4.z;
        tmp.w = value.vec4.w;
        updateVec4 (tmp);
        break;
    }
    case UNIFORM_INT:
        updateInt (value.i);
        break;
    case UNIFORM_FLOAT:
        updateFloat (value.f);
        break;
    case UNIFORM_BOOLEAN:
        updateBool (value.b);
        break;
    case NO_TYPE:
    case LAST_TYPE:
        assert (false);
        break;
    }

    shader->unbind();
}

ShaderUniformType ShaderUniform::getUniformTypeFromName(const char * name) {
    return uniformTypesStringMap[name];
}

ShaderUniformType ShaderUniform::getUniformType () {
    return type;
}

ShaderUniformValue ShaderUniform::getUniformValue () {
    return value;
}

ShaderUniform::~ShaderUniform() {

}
