#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <GL/glew.h>
#include <iostream>
#include "ErrorUtils.h"

class Texture
{
    public:
        Texture (const std::string &filename,
                 int textureUnit,
                 int numOfSubTxtsW = 1,
                 int numOfSubTxtsH = 1);
        Texture (GLuint textureId = -1, int textureUnit = -1,
                 int numOfSubTxtsW = 1,
                 int numOfSubTxtsH = 1);
        ~Texture();

        ErrorCode init(const std::string &filename,
                       int textureUnit,
                       int numOfSubTxtsW = 1,
                       int numOfSubTxtsH = 1);
        ErrorCode init(GLuint textureId,
                       int textureUnit,
                       int numOfSubTxtsW = 1,
                       int numOfSubTxtsH = 1);

        void use();
        void use(int textureUnit);

        GLuint getTextureId();
        int getTextureUnit();
        int getNumOfSubTxtsW ();
        int getNumOfSubTxtsH ();
        int getTotalNumOfSubTxts ();
        float getSubWidth ();
        float getSubHeight ();

        const std::string &getFilePath() { return filePath; }

    protected:
    private:
        GLuint textureId;
        int textureUnit;
        int numOfSubTxtsW, numOfSubTxtsH, totalNumOfSubTxts;
        float subWidth, subHeight;

        std::string filePath;
};

#endif // TEXTURE_H
