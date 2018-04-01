#ifndef FONT_H
#define FONT_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <string.h>
#include <sstream>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <SDL2/SDL.h>

#define MAX_ASCII 300

struct Character{
    Character(char id, int x, int y, int width, int height, int xoffset, int yoffset, int xadvance) : id(id),x(x),y(y),width(width),height(height),xoffset(xoffset),yoffset(yoffset),xadvance(xadvance){}
    Character() : Character(-1,0,0,0,0,0,0,0){}

    char id;
    int x, y;
    int width, height;
    int xoffset, yoffset;
    int xadvance;
};

class Font
{
    public:
        Font (const std::string& filename, const std::string& atlas);
        const std::vector<Character> &getChars ();
        GLuint getTextureId ();
        int getWidth ();
        int getHeight ();
        static Font &getNormalFont ();
        virtual ~Font();
    protected:
    private:
        int getNumber (std::string inputString);
        int scaleW, scaleH;
        std::vector<Character> charsInfo;
        GLuint textureId;
};

#endif // FONT_H
