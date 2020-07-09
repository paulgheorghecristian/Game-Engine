#include "Font.h"

Font::Font(const std::string& fontFilename, const std::string& fontAtlasName) : charsInfo(MAX_ASCII, Character())
{
    std::ifstream file(fontFilename.c_str(), std::ios::in | std::ios::binary);

    if(!file.good()){
        std::cout << "Nu am gasit fisierul " << fontFilename << std::endl;
        std::terminate();
    }

    std::string line;

    std::getline(file, line);
    std::getline(file, line);

    char *str = strdup(line.c_str());
    char *p = strstr(str, "scaleW=");
    char *q = strstr(str, "scaleH=");

    if(p && q){
        p += 7;
        q += 7;

        char *aux = p;
        while(*aux != ' '){
            aux++;
        }
        *aux = '\0';
        scaleW = atoi(p);

        aux = q;
        while(*aux != ' '){
            aux++;
        }
        *aux = '\0';
        scaleH = atoi(q);
    }
    std::getline(file, line);
    std::getline(file, line);

    while(std::getline(file,line)){
        std::stringstream ss(line, std::ios::in);
        std::string token;
        ss >> token;
        ss >> token;
        int id = getNumber(token);
        ss >> token;
        int x = getNumber(token);
        ss >> token;
        int y = getNumber(token);
        ss >> token;
        int width = getNumber(token);
        ss >> token;
        int height = getNumber(token);
        ss >> token;
        int xoffset = getNumber(token);
        ss >> token;
        int yoffset = getNumber(token);
        ss >> token;
        int xadvance = getNumber(token);

        charsInfo[id] = Character(id, x, y, width, height, xoffset, yoffset, xadvance);
    }

    file.close();
    delete str;

    SDL_Surface *fontAtlas = SDL_LoadBMP(fontAtlasName.c_str());

    if (fontAtlas == NULL) {
        std::cerr << "Loading font atlas error!" << std::endl;
        exit(1);
    }
		//genereaza textura
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, fontAtlas->w, fontAtlas->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fontAtlas->pixels);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.7f);

    SDL_FreeSurface(fontAtlas);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint Font::getTextureId() {
    return textureId;
}

int Font::getNumber (std::string str) {
    char *p = strdup(str.c_str());
    char *q = strchr(p, '=');
    q += 1;
    int number = atoi(q);
    delete p;
    return number;
}

const std::vector<Character> &Font::getChars() {
    return charsInfo;
}

int Font::getWidth() {
    return scaleW;
}

int Font::getHeight() {
    return scaleH;
}

Font &Font::getNormalFont() {
    static Font font ("res/fonts/normalFont.fnt", "res/fonts/normalFont.bmp");

    return font;
}

Font::~Font() {
    glDeleteTextures(1, &textureId);
}
