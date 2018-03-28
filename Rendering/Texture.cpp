#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

using uint = unsigned int;
using uchar = unsigned char;

#include "stb_image.h"
#include "stb_image_write.h"

const static GLint pixelFormat[5] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };
const static GLint internalFormat[][5] = {
                                            { 0, GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 },
                                            { 0, GL_R16, GL_RG16, GL_RGB16, GL_RGBA16 },
                                            { 0, GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F },
                                            { 0, GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F }
                                        };

Texture::Texture(const std::string& textureFilename,
                 int textureUnit,
                 int numOfSubTxtsW,
                 int numOfSubTxtsH) : textureUnit (textureUnit),
                                      numOfSubTxtsH (numOfSubTxtsH),
                                      numOfSubTxtsW (numOfSubTxtsW),
                                      totalNumOfSubTxts (numOfSubTxtsH * numOfSubTxtsW) {
    int width, height, chn;
	unsigned char *data = stbi_load (textureFilename.c_str (), &width, &height, &chn, 0);

	if (numOfSubTxtsH < numOfSubTxtsW) {
        subWidth = 1.0f / numOfSubTxtsW;
        subHeight = 1.0f / numOfSubTxtsW;
	} else {
        subWidth = 1.0f / numOfSubTxtsH;
        subHeight = 1.0f / numOfSubTxtsH;
	}

    if (data == NULL) {
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
                  internalFormat[0][chn],
                  width,
                  height,
                  0,
                  pixelFormat[chn],
                  GL_UNSIGNED_BYTE,
                  data);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.7f);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
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

const int &Texture::getNumOfSubTxtsW () {
    return numOfSubTxtsW;
}

const int &Texture::getNumOfSubTxtsH () {
    return numOfSubTxtsH;
}

const int &Texture::getTotalNumOfSubTxts () {
    return totalNumOfSubTxts;
}

const float &Texture::getSubWidth () {
    return subWidth;
}

const float &Texture::getSubHeight () {
    return subHeight;
}

void Texture::use(){
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId);
}
