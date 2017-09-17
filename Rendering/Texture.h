#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>

class Texture
{
    public:
        Texture(std::string, int);
        Texture(GLuint, int);
        GLuint getTextureId();
        const int &getTextureUnit();
        void use();
        virtual ~Texture();
    protected:
    private:
        GLuint textureId;
        int textureUnit;
};

#endif // TEXTURE_H
