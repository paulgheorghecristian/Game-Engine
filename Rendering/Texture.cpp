#include "Texture.h"

#include "ErrorUtils.h"

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

Texture::Texture(const std::string &textureFilename,
                 int textureUnit,
                 int numOfSubTxtsW,
                 int numOfSubTxtsH)
{
    ErrorCode ec;

    ec = init(textureFilename, textureUnit, numOfSubTxtsW, numOfSubTxtsH);
    assert(ec == NO_ERROR);
}

Texture::Texture(GLuint textureId, int textureUnit,
                 int numOfSubTxtsW,
                 int numOfSubTxtsH)
{
    (void) init(textureId, textureUnit);
}

ErrorCode Texture::init(const std::string &filename,
                       int textureUnit,
                       int numOfSubTxtsW,
                       int numOfSubTxtsH)
{
    int width, height, chn;
	unsigned char *data = stbi_load (filename.c_str(), &width, &height, &chn, 0);

	if (data == NULL) {
        print_error(FILE_NOT_FOUND, filename.c_str());
        return FILE_NOT_FOUND;
    }

    this->textureUnit = textureUnit;
    this->numOfSubTxtsH = numOfSubTxtsH;
    this->numOfSubTxtsW = numOfSubTxtsW;
    this->totalNumOfSubTxts = numOfSubTxtsH * numOfSubTxtsW;

	if (numOfSubTxtsH < numOfSubTxtsW) {
        subWidth = 1.0f / numOfSubTxtsW;
        subHeight = 1.0f / numOfSubTxtsW;
	} else {
        subWidth = 1.0f / numOfSubTxtsH;
        subHeight = 1.0f / numOfSubTxtsH;
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

    print_error(NO_ERROR, "Loading", filename.c_str());
    return NO_ERROR;
}

ErrorCode Texture::init(GLuint textureId,
                        int textureUnit,
                        int numOfSubTxtsW,
                        int numOfSubTxtsH)
{
    this->textureId = textureId;
    this->textureUnit = textureUnit;
    this->numOfSubTxtsH = numOfSubTxtsH;
    this->numOfSubTxtsW = numOfSubTxtsW;
    this->totalNumOfSubTxts = numOfSubTxtsH * numOfSubTxtsW;

    return NO_ERROR;
}

Texture::~Texture()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &textureId);
}

GLuint Texture::getTextureId(){
    return textureId;
}

int Texture::getTextureUnit(){
    return textureUnit;
}

int Texture::getNumOfSubTxtsW () {
    return numOfSubTxtsW;
}

int Texture::getNumOfSubTxtsH () {
    return numOfSubTxtsH;
}

int Texture::getTotalNumOfSubTxts () {
    return totalNumOfSubTxts;
}

float Texture::getSubWidth () {
    return subWidth;
}

float Texture::getSubHeight () {
    return subHeight;
}

void Texture::use()
{
    Texture::use(textureUnit);
}

void Texture::use(int textureUnit)
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId);
}
