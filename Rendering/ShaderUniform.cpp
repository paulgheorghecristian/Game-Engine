#include "ShaderUniform.h"

#include <cstring>

ShaderUniform::ShaderUniform(const std::string &name, const char * t, Shader * shader) : type (getUniformTypeFromName (t)),
                                                                                            hasValue (false),
                                                                                            shader (shader),
                                                                                            name (name) {
    memset(&value, 0, sizeof(value));
    location = shader->getUniformLocation (name);
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
    typesMap["MAT4s"] = UNIFORM_MAT4s;

    return typesMap;
}

std::unordered_map<std::string, ShaderUniformType> ShaderUniform::uniformTypesStringMap = ShaderUniform::getUniformTypesStringMap ();

static size_t uniformValueSize(ShaderUniformType type) {
    switch (type) {
    case UNIFORM_MAT4:    return sizeof(float) * 16;
    case UNIFORM_VEC2:    return sizeof(float) * 2;
    case UNIFORM_VEC3:    return sizeof(float) * 3;
    case UNIFORM_VEC4:    return sizeof(float) * 4;
    case UNIFORM_FLOAT:   return sizeof(float);
    case UNIFORM_INT:     return sizeof(int);
    case UNIFORM_BOOLEAN: return sizeof(bool);
    default:              return 0;
    }
}

/* Upload the cached value to GL. Uses glProgramUniform* when available so the
 * program doesn't have to be bound (and later rebound); otherwise binds the
 * program through Shader::bind, which tracks the currently bound program. */
void ShaderUniform::upload () {
    const GLuint program = shader->getShaderId();
    const bool dsa = Shader::hasDSAUniforms();

    if (!dsa) {
        shader->bind();
    }

    switch (type) {
    case UNIFORM_MAT4:
        if (dsa) glProgramUniformMatrix4fv(program, location, 1, GL_FALSE, value.mat4);
        else     glUniformMatrix4fv(location, 1, GL_FALSE, value.mat4);
        break;
    case UNIFORM_VEC2:
        if (dsa) glProgramUniform2f(program, location, value.vec2.x, value.vec2.y);
        else     glUniform2f(location, value.vec2.x, value.vec2.y);
        break;
    case UNIFORM_VEC3:
        if (dsa) glProgramUniform3f(program, location, value.vec3.x, value.vec3.y, value.vec3.z);
        else     glUniform3f(location, value.vec3.x, value.vec3.y, value.vec3.z);
        break;
    case UNIFORM_VEC4:
        if (dsa) glProgramUniform4f(program, location, value.vec4.x, value.vec4.y, value.vec4.z, value.vec4.w);
        else     glUniform4f(location, value.vec4.x, value.vec4.y, value.vec4.z, value.vec4.w);
        break;
    case UNIFORM_FLOAT:
        if (dsa) glProgramUniform1f(program, location, value.f);
        else     glUniform1f(location, value.f);
        break;
    case UNIFORM_INT:
        if (dsa) glProgramUniform1i(program, location, value.i);
        else     glUniform1i(location, value.i);
        break;
    case UNIFORM_BOOLEAN:
        if (dsa) glProgramUniform1i(program, location, (int) value.b);
        else     glUniform1i(location, (int) value.b);
        break;
    default:
        break;
    }
    hasValue = true;
}

void ShaderUniform::updateUniform (void * data) {
    assert (type < LAST_TYPE);

    size_t size = uniformValueSize(type);
    if (size == 0) {
        return;
    }

    /* skip redundant uploads (material/sampler uniforms repeat every draw) */
    if (hasValue && memcmp(&value, data, size) == 0) {
        return;
    }

    memcpy(&value, data, size);
    upload();
}

void ShaderUniform::updateUniform(const std::vector<glm::mat4> &mats) {
    assert(type == UNIFORM_MAT4s);

    if (mats.empty()) {
        return;
    }

    if (Shader::hasDSAUniforms()) {
        glProgramUniformMatrix4fv(shader->getShaderId(), location, mats.size(),
                                  GL_FALSE, glm::value_ptr(mats[0]));
    } else {
        shader->bind();
        glUniformMatrix4fv(location, mats.size(), GL_FALSE, glm::value_ptr(mats[0]));
    }
}

void ShaderUniform::reload () {
    assert (shader != NULL);

    /* the program may have been re-linked: refresh the location */
    location = shader->getUniformLocation (name);

    if (type == UNIFORM_MAT4s || type == NO_TYPE || type >= LAST_TYPE) {
        return;
    }
    /* uniforms that were never set keep GL's default (zero), as before */
    if (hasValue) {
        upload();
    }
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
