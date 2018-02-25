#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <GL/glew.h>
#include <iostream>

class Texture
{
    public:
        Texture (const std::string &filename,
                 int textureUnit,
                 int numOfSubTxtsW = 1,
                 int numOfSubTxtsH = 1);
        Texture (GLuint textureId, int textureUnit);
        GLuint getTextureId();
        const int &getTextureUnit();
        void use();
        virtual ~Texture();

        const int &getNumOfSubTxtsW ();
        const int &getNumOfSubTxtsH ();
        const int &getTotalNumOfSubTxts ();
        const float &getSubWidth ();
        const float &getSubHeight ();

    protected:
    private:
        GLuint textureId;
        int textureUnit;
        int numOfSubTxtsW, numOfSubTxtsH, totalNumOfSubTxts;
        float subWidth, subHeight;
};

#endif // TEXTURE_H
