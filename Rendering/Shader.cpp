#include "Shader.h"

GLuint Shader::currentProgram = 0;

bool Shader::hasDSAUniforms() {
    static int cached = -1;
    if (cached < 0) {
        cached = (GLEW_VERSION_4_1 || GLEW_ARB_separate_shader_objects) ? 1 : 0;
    }
    return cached == 1;
}

Shader::Shader() : program(0)
{

}

Shader::Shader (const std::string &jsonPath) : shaderPath (jsonPath), program(0)
{
    construct(jsonPath);
}

Shader *Shader::construct (const std::string &jsonPath)
{
    GLint compileResult = 0, linkResult = 0;
    char infoLogMessage[1024];
    std::string jsonBody;
    std::string vertexShaderSource;
    std::string fragmentShaderSource;
    std::string geometryShaderSource;
    rapidjson::Document jsonShaderDocument;
    rapidjson::ParseResult parseResult;

    shaderPath = jsonPath;

    jsonBody = FileUtils::loadFileInString (jsonPath);
    parseResult = jsonShaderDocument.Parse (jsonBody.c_str ());
    if (!parseResult) {
        std::cout << "JSON Shader (" << jsonPath << ") parse error: " << rapidjson::GetParseError_En (parseResult.Code ()) << " (" << parseResult.Offset() << ")" << std::endl;;
        exit (-1);
    }

    if (!jsonShaderDocument.HasMember ("VSPath") ||
        !jsonShaderDocument.HasMember ("FSPath")) {
        std::cout << "Incomplete JSON: " << jsonPath << std::endl;
        exit (-1);
    }

    vertexShaderSource = FileUtils::loadFileInString (jsonShaderDocument["VSPath"].GetString ());
    fragmentShaderSource = FileUtils::loadFileInString (jsonShaderDocument["FSPath"].GetString ());

    const GLchar * vertexShaderSourceP[1];
    const GLchar * fragmentShaderSourceP[1];

    GLint vertexLength[1];
    GLint fragmentLength[1];

    vertexShaderSourceP[0] = vertexShaderSource.c_str ();
    fragmentShaderSourceP[0] = fragmentShaderSource.c_str ();

    vertexLength[0] = strlen (vertexShaderSourceP[0]);
    fragmentLength[0] = strlen (fragmentShaderSourceP[0]);

    vertexHandle = glCreateShader (GL_VERTEX_SHADER);
    glShaderSource (vertexHandle, 1, vertexShaderSourceP, vertexLength);
    glCompileShader (vertexHandle);

    glGetShaderiv (vertexHandle, GL_COMPILE_STATUS, &compileResult);
	if (compileResult == GL_FALSE) {
        glGetShaderInfoLog (vertexHandle, 1024, NULL, infoLogMessage);
        std::cout << "EROARE COMPILARE vertex shader[" << jsonShaderDocument["VSPath"].GetString () << "]"  << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit (-1);
	}

	fragmentHandle = glCreateShader (GL_FRAGMENT_SHADER);
    glShaderSource (fragmentHandle, 1, fragmentShaderSourceP, fragmentLength);
    glCompileShader (fragmentHandle);

    glGetShaderiv (fragmentHandle, GL_COMPILE_STATUS, &compileResult);
	if (compileResult == GL_FALSE) {
        glGetShaderInfoLog (fragmentHandle, 1024, NULL, infoLogMessage);
        std::cout << "EROARE COMPILARE fragment shader[" << jsonShaderDocument["FSPath"].GetString () << "]" << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit (-1);
	}

    if (jsonShaderDocument.HasMember ("GSPath")){
        geometryShaderSource = FileUtils::loadFileInString (jsonShaderDocument["GSPath"].GetString ());
        const GLchar* geometryShaderSourceP[1];
        GLint geometryLength[1];
        geometryShaderSourceP[0] = geometryShaderSource.c_str ();
        geometryLength[0] = strlen (geometryShaderSourceP[0]);

        geometryHandle = glCreateShader (GL_GEOMETRY_SHADER);
        glShaderSource (geometryHandle, 1, geometryShaderSourceP, geometryLength);
        glCompileShader (geometryHandle);

        glGetShaderiv (geometryHandle, GL_COMPILE_STATUS, &compileResult);
        if (compileResult == GL_FALSE) {
            glGetShaderInfoLog (geometryHandle, 1024, NULL, infoLogMessage);
            std::cout << "EROARE COMPILARE geometry shader[" << jsonShaderDocument["GSPath"].GetString () << "]" << std::endl << "LOG=" << infoLogMessage << std::endl;
            exit (-1);
        }
	}

    program = glCreateProgram ();

    glAttachShader (program, vertexHandle);
    glAttachShader (program, fragmentHandle);

    if (jsonShaderDocument.HasMember ("GSPath")){
        glAttachShader(program, geometryHandle);
    }
    glLinkProgram (program);

    glGetProgramiv (program, GL_LINK_STATUS, &linkResult);
    if (linkResult == GL_FALSE) {
		glGetProgramInfoLog(program, 1024, NULL, infoLogMessage);
        std::cout << "EROARE LINK program shader[" << jsonPath << "]" << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit(-1);
	}

    if (jsonShaderDocument.HasMember ("Uniforms")) {
        rapidjson::Value uniformsObject;

        uniformsObject = jsonShaderDocument["Uniforms"];
        for (rapidjson::Value::ConstMemberIterator itr = uniformsObject.MemberBegin (); itr != uniformsObject.MemberEnd (); ++itr) {
            if (findUniform(itr->name.GetString ()) == NULL) {
                ShaderUniform * uniform = new ShaderUniform (itr->name.GetString (), itr->value.GetString (), this);
                uniforms[itr->name.GetString ()] = uniform;
            }
        }
    }

    glDetachShader (program, vertexHandle);
    glDetachShader (program, fragmentHandle);
    if (jsonShaderDocument.HasMember ("GSPath")){
        glDetachShader (program, geometryHandle);
    }

    glDeleteShader (vertexHandle);
	glDeleteShader (fragmentHandle);
	if (jsonShaderDocument.HasMember ("GSPath")){
        glDeleteShader (geometryHandle);
	}

	/* (re)query locations and (re)upload cached values; needed after reload()
	 * since the new program's uniform locations can differ */
	for (auto &it : uniforms) {
        it.second->reload ();
	}

	return this;
}

void Shader::unbind() {
    /* Intentionally a no-op: every draw binds its own program first, so
     * switching to program 0 in between only costs driver validation. */
}

void Shader::bind() {
    if (program > 0 && program != currentProgram) {
        glUseProgram(program);
        currentProgram = program;
    }
}

void Shader::reload() {
    glUseProgram (0);
    currentProgram = 0;
    if (program > 0) {
        glDeleteProgram (program);
    }

    construct (shaderPath);
}

ShaderUniform *Shader::findUniform(std::string_view name) {
    auto it = uniforms.find(name);
    return it == uniforms.end() ? NULL : it->second;
}

bool Shader::updateUniform (std::string_view name, void * data) {
    ShaderUniform *uniform = findUniform(name);

    if (uniform != NULL) {
        uniform->updateUniform (data);
        return true;
    }
    return false;
}

bool Shader::updateUniform (std::string_view name, int data) {
    ShaderUniform *uniform = findUniform(name);

    if (uniform != NULL) {
        assert (uniform->getUniformType() == UNIFORM_INT);
        uniform->updateUniform (&data);
        return true;
    }
    return false;
}

bool Shader::updateUniform (std::string_view name, float data) {
    ShaderUniform *uniform = findUniform(name);

    if (uniform != NULL) {
        assert (uniform->getUniformType() == UNIFORM_FLOAT);
        uniform->updateUniform (&data);
        return true;
    }
    return false;
}

bool Shader::updateUniform (std::string_view name, bool data) {
    ShaderUniform *uniform = findUniform(name);

    if (uniform != NULL) {
        assert (uniform->getUniformType() == UNIFORM_BOOLEAN);
        uniform->updateUniform(&data);
        return true;
    }
    return false;
}

bool Shader::updateUniform(std::string_view name, const std::vector<glm::mat4> &mats) {
    ShaderUniform *uniform = findUniform(name);

    if (uniform != NULL) {
        assert(uniform->getUniformType() == UNIFORM_MAT4s);
        uniform->updateUniform(mats);
        return true;
    }
    assert(false);
    return false;
}

GLuint Shader::getUniformLocation(const std::string &name) {
    return glGetUniformLocation (program, name.c_str ());
}

Shader::~Shader()
{
    for (auto it : uniforms) {
        delete it.second;
    }

    if (program > 0) {
        if (currentProgram == program) {
            currentProgram = 0;
        }
        glDeleteProgram (program);
    }
}
