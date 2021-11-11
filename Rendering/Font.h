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
#include <unordered_map>
#include <tuple>


typedef std::tuple<int, int> kerningsKey_t;

struct key_hash : public std::unary_function<kerningsKey_t, std::size_t>
{
   std::size_t operator()(const kerningsKey_t& k) const
   {
      return std::get<0>(k) ^ std::get<1>(k);
   }
};

struct key_equal : public std::binary_function<kerningsKey_t, kerningsKey_t, bool>
{
   bool operator()(const kerningsKey_t& v0, const kerningsKey_t& v1) const
   {
      return (
               std::get<0>(v0) == std::get<0>(v1) &&
               std::get<1>(v0) == std::get<1>(v1)
             );
   }
};

typedef std::unordered_map<kerningsKey_t, int, key_hash, key_equal> kerningsMap_t;


struct Character{
    Character(int x, int y, int width, int height, int xoffset, int yoffset, int xadvance) :
                x(x),y(y),width(width),height(height),xoffset(xoffset),yoffset(yoffset),xadvance(xadvance){}
    Character() {}

    int x, y;
    int width, height;
    int xoffset, yoffset;
    int xadvance;
};

class Font
{
    public:
        Font (const std::string& filename, const std::string& atlas);
        std::unordered_map<int, Character> &getChars ();
        kerningsMap_t &getKernings();
        GLuint getTextureId ();
        int getWidth ();
        int getHeight ();

        static std::unordered_map<std::string, Font *> fontsCache;
        static Font *getFontFromCache(const std::string &path);
        static void addFontToCache(const std::string &path);
        virtual ~Font();
    protected:
    private:
        int getNumber (const std::string &str);
        int scaleW, scaleH;
        std::unordered_map<int, Character> charsInfo;
        kerningsMap_t kernings;
        GLuint textureId;
};

#endif // FONT_H
