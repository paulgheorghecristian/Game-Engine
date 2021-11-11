#include "Font.h"

std::unordered_map<std::string, Font *> Font::fontsCache;

Font::Font(const std::string& fontFilename, const std::string& fontAtlasName)
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
        if (line.find("kernings") != std::string::npos) {
            break;
        }
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

        charsInfo[id] = Character(x, y, width, height, xoffset, yoffset, xadvance);
    }

    while (std::getline(file,line)) {
        std::stringstream ss(line, std::ios::in);
        std::string token;

        ss >> token;
        ss >> token;

        int first = getNumber(token);
        ss >> token;
        int second = getNumber(token);
        ss >> token;
        int amount = getNumber(token);

        kernings[std::make_tuple(first, second)] = amount;
    }

    file.close();
    free(str);

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

int Font::getNumber(const std::string &str) {
    char *p = strdup(str.c_str());
    char *q = strchr(p, '=');
    q += 1;
    int number = atoi(q);
    free(p);
    return number;
}

std::unordered_map<int, Character> &Font::getChars() {
    return charsInfo;
}

kerningsMap_t &Font::getKernings() {
    return kernings;
}

int Font::getWidth() {
    return scaleW;
}

int Font::getHeight() {
    return scaleH;
}

Font *Font::getFontFromCache(const std::string &path) {
    if (fontsCache.find(path) == fontsCache.end())
        return nullptr;

    return fontsCache[path];
}

void Font::addFontToCache(const std::string &path) {
    fontsCache[path] = new Font(path+".fnt", path+".bmp");
}

Font::~Font() {
    glDeleteTextures(1, &textureId);
}
