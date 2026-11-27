#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <GL/glew.h>
#include <iostream>
#include "ErrorUtils.h"

class Texture
{
    public:
        /* a picture decoded ahead, on a worker thread (a level streaming in: its textures read and
         * decoded off the game's thread, so that building it only uploads them) */
        static void preload(const std::string &filename);
        /* whether it is ready to be made at once (already made, or decoded ahead) */
        static bool ready(const std::string &filename);
        /* the big ones decoded ahead go up to the GPU in strips, a few rows a frame (one 4K picture at once
         * held a frame up for tens of milliseconds): this frame's share, until `budgetMs` is spent */
        static void pumpUploads(double budgetMs);
        /* pictures decoded ahead that were never made into textures (a level taken out, the game
         * quitting): their pixels freed */
        static void dropPreloaded();
        static void dropPreloaded(const std::string &filename);
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
        GLuint textureId = 0;           /* (0 until made: a missing file deleted another texture's id) */
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
