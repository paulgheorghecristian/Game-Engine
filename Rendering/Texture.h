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

        /* Image files are decoded and uploaded once: textures loaded from the
         * same path share one GL texture (reference counted). Each Texture
         * object still has its own texture unit / atlas settings. */
        bool m_shared = false;

        /* sharing would make a copy release the GL texture twice */
        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;
};

#endif // TEXTURE_H
