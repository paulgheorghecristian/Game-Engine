#include "Shader.h"

Shader::Shader(std::string &&jsonPath) {

    GLint compileResult = 0, linkResult = 0;
    char infoLogMessage[1024];
    std::string jsonBody;
    std::string vertexShaderSource;
    std::string fragmentShaderSource;
    std::string geometryShaderSource;
    rapidjson::Document jsonShaderDocument;
    rapidjson::ParseResult parseResult;

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
        std::cout << "EROARE COMPILARE vertex shader[" << vertexShaderPath << "]"  << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit (-1);
	}

	fragmentHandle = glCreateShader (GL_FRAGMENT_SHADER);
    glShaderSource (fragmentHandle, 1, fragmentShaderSourceP, fragmentLength);
    glCompileShader (fragmentHandle);

    glGetShaderiv (fragmentHandle, GL_COMPILE_STATUS, &compileResult);
	if (compileResult == GL_FALSE) {
        glGetShaderInfoLog (fragmentHandle, 1024, NULL, infoLogMessage);
        std::cout << "EROARE COMPILARE fragment shader[" << fragmentShaderPath << "]" << std::endl << "LOG=" << infoLogMessage << std::endl;
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
            std::cout << "EROARE COMPILARE geometry shader[" << geometryShaderPath << "]" << std::endl << "LOG=" << infoLogMessage << std::endl;
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
        std::cout << "EROARE LINK program shader[" << vertexShaderPath << "]" << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit(-1);
	}

    if (jsonShaderDocument.HasMember ("Uniforms")) {
        rapidjson::Value uniformsObject;

        uniformsObject = jsonShaderDocument["Uniforms"];
        for (rapidjson::Value::ConstMemberIterator itr = uniformsObject.MemberBegin (); itr != uniformsObject.MemberEnd (); ++itr) {
            ShaderUniform * uniform = new ShaderUniform (itr->name.GetString (), itr->value.GetString (), this);
            uniforms[itr->name.GetString ()] = uniform;
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

	std::cout << jsonPath << std::endl;
	for (auto it : uniforms) {
        std::cout << it.first << " " << it.second << std::endl;
	}
}

void Shader::unbind() {
    glUseProgram (0);
}

void Shader::bind() {
    glUseProgram (program);
}

void Shader::reloadShader() {
    /* TODO ADD GEOM SHAD*/
    /*glUseProgram(0);
    glDeleteProgram (program);

    std::string vertexShaderSource = loadShader(vertexShaderPath);
    std::string fragmentShaderSource = loadShader(fragmentShaderPath);

    GLint compileResult = 0, linkResult = 0;
    char infoLogMessage[1024];

    const GLchar* vertexShaderSourceP[1];
    const GLchar* fragmentShaderSourceP[1];
    GLint vertexLength[1];
    GLint fragmentLength[1];

    vertexShaderSourceP[0] = vertexShaderSource.c_str();
    fragmentShaderSourceP[0] = fragmentShaderSource.c_str();

    vertexLength[0] = strlen(vertexShaderSourceP[0]);
    fragmentLength[0] = strlen(fragmentShaderSourceP[0]);

    GLuint vertexHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexHandle, 1, vertexShaderSourceP, vertexLength);
    glCompileShader(vertexHandle);

    glGetShaderiv(vertexHandle, GL_COMPILE_STATUS, &compileResult);
	if(compileResult == GL_FALSE) {
        glGetShaderInfoLog(vertexHandle, 1024, NULL, infoLogMessage);
        std::cout << "EROARE COMPILARE vertex shader" << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit(-1);
	}

	GLuint fragmentHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentHandle, 1, fragmentShaderSourceP, fragmentLength);
    glCompileShader(fragmentHandle);

    glGetShaderiv(fragmentHandle, GL_COMPILE_STATUS, &compileResult);
	if(compileResult == GL_FALSE) {
        glGetShaderInfoLog(fragmentHandle, 1024, NULL, infoLogMessage);
        std::cout << "EROARE COMPILARE fragment shader" << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit(-1);
	}

    program = glCreateProgram ();

    glAttachShader (program, vertexHandle);
    glAttachShader (program, fragmentHandle);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &linkResult);
    if (linkResult == GL_FALSE) {
		glGetProgramInfoLog (program, 1024, NULL, infoLogMessage);
        std::cout << "EROARE LINK program shader" << std::endl << "LOG=" << infoLogMessage << std::endl;
        exit(-1);
	}

    glDetachShader (program, vertexHandle);
    glDetachShader (program, fragmentHandle);

    glDeleteShader(vertexHandle);
	glDeleteShader(fragmentHandle);

	glUseProgram(program);*/
}

bool Shader::updateUniform (const std::string &name, void * data) {
    ShaderUniform * uniform = uniforms[name];

    if (uniform) {
        uniform->updateUniform (data);
        return true;
    } else {
        return false;
    }
}

GLuint Shader::getUniformLocation(const std::string &name) {
    return glGetUniformLocation (program, name.c_str ());
}

Shader::~Shader()
{
    for (auto it : uniforms) {
        delete it.second;
    }
}
