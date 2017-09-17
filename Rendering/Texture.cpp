#include "texture.h"

Texture::Texture(std::string textureFilename, int textureUnit) : textureUnit (textureUnit) {
    SDL_Surface *texture = SDL_LoadBMP(textureFilename.c_str());

    if(!texture) {
        std::cerr << "Loading texture " << textureFilename << " error!" << std::endl;
        exit(1);
    }

    //genereaza textura
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_RGB,
                  texture->w,
                  texture->h,
                  0,
                  GL_BGR,
                  GL_UNSIGNED_BYTE,
                  texture->pixels);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.7f);

    glBindTexture(GL_TEXTURE_2D, 0);
    SDL_FreeSurface(texture);
}

Texture::Texture(GLuint textureId, int textureUnit) : textureId(textureId), textureUnit(textureUnit)
{

}

Texture::~Texture()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &textureId);
}

GLuint Texture::getTextureId(){
    return textureId;
}

const int &Texture::getTextureUnit(){
    return textureUnit;
}

void Texture::use(){
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId);
}
