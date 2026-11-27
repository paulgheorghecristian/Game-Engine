#include <algorithm>
#include "Texture.h"

#include "ErrorUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

using uint = unsigned int;
using uchar = unsigned char;

#include "stb_image.h"
#include "stb_image_write.h"

#include <chrono>
#include <future>
#include <semaphore>
#include <vector>
#include <unordered_map>

const static GLint pixelFormat[5] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };
const static GLint internalFormat[][5] = {
                                            { 0, GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 },
                                            { 0, GL_R16, GL_RG16, GL_RGB16, GL_RGBA16 },
                                            { 0, GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F },
                                            { 0, GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F }
                                        };

struct SharedTexture {
    GLuint id;
    int references;
};

static std::unordered_map<std::string, SharedTexture> &textureCache() {
    static std::unordered_map<std::string, SharedTexture> cache;
    return cache;
}

/* pictures being decoded ahead (Texture::preload): what stb_image gives, on a worker thread */
struct Decoded {
    unsigned char *data = nullptr;
    int width = 0, height = 0, channels = 0;
};
static std::unordered_map<std::string, std::shared_future<Decoded>> &decoding() {
    static std::unordered_map<std::string, std::shared_future<Decoded>> pending;
    return pending;
}

void Texture::preload(const std::string &filename)
{
    if (textureCache().count(filename) || decoding().count(filename)) return;
    stbi_set_flip_vertically_on_load(true);         /* (the same for every load: set before any thread) */
    decoding()[filename] = std::async(std::launch::async, [filename]() {
        /* a few at a time (a level's pictures all decoded at once: a thread each, all of it in memory) */
        static std::counting_semaphore<4> slots(4);
        slots.acquire();
        stbi_set_flip_vertically_on_load_thread(1);
        Decoded d;
        d.data = stbi_load(filename.c_str(), &d.width, &d.height, &d.channels, 0);
        slots.release();
        return d;
    }).share();
}

/* big pictures going up in strips (Texture::pumpUploads): their texture made, filled row by row */
struct StripUpload {
    GLuint id;
    unsigned char *data;
    int width, height, channels, nextRow;
};
static std::vector<StripUpload> &stripUploads() {
    static std::vector<StripUpload> uploads;
    return uploads;
}

void Texture::pumpUploads(double budgetMs)
{
    if (stripUploads().empty()) return;
    auto start = std::chrono::steady_clock::now();
    GLint alignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    auto &uploads = stripUploads();
    while (!uploads.empty() &&
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() < budgetMs) {
        StripUpload &up = uploads.front();
        int rows = std::min(128, up.height - up.nextRow);
        glBindTexture(GL_TEXTURE_2D, up.id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, up.nextRow, up.width, rows, pixelFormat[up.channels], GL_UNSIGNED_BYTE,
                        up.data + (size_t) up.nextRow * up.width * up.channels);
        up.nextRow += rows;
        if (up.nextRow >= up.height) {
            glGenerateMipmap(GL_TEXTURE_2D);    /* (all there: its smaller levels) */
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
            stbi_image_free(up.data);
            uploads.erase(uploads.begin());
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
}

void Texture::dropPreloaded(const std::string &filename)
{
    auto pending = decoding().find(filename);
    if (pending == decoding().end()) return;
    stbi_image_free(pending->second.get().data);
    decoding().erase(pending);
}

void Texture::dropPreloaded()
{
    for (auto &pending : decoding()) stbi_image_free(pending.second.get().data);
    decoding().clear();
    for (StripUpload &up : stripUploads()) stbi_image_free(up.data);
    stripUploads().clear();
}

bool Texture::ready(const std::string &filename)
{
    if (textureCache().count(filename)) return true;
    auto pending = decoding().find(filename);
    return pending == decoding().end() ||        /* (not asked for ahead: made the usual way) */
           pending->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}


Texture::Texture(const std::string &textureFilename,
                 int textureUnit,
                 int numOfSubTxtsW,
                 int numOfSubTxtsH) : filePath(textureFilename)
{
    ErrorCode ec;

    ec = init(textureFilename, textureUnit, numOfSubTxtsW, numOfSubTxtsH);
    assert(ec == NO_ERROR);
}

Texture::Texture(GLuint textureId, int textureUnit,
                 int numOfSubTxtsW,
                 int numOfSubTxtsH) : filePath("")
{
    (void) init(textureId, textureUnit);
}

ErrorCode Texture::init(const std::string &filename,
                       int textureUnit,
                       int numOfSubTxtsW,
                       int numOfSubTxtsH)
{
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

    auto cached = textureCache().find(filename);
    if (cached != textureCache().end()) {
        cached->second.references++;
        textureId = cached->second.id;
        m_shared = true;
        return NO_ERROR;
    }

    stbi_set_flip_vertically_on_load(true);

    int width, height, chn;
    unsigned char *data;
    bool decodedAhead = false;
    auto pending = decoding().find(filename);
    if (pending != decoding().end()) {
        decodedAhead = true;
        /* decoded ahead (waits if it is not done yet) */
        Decoded d = pending->second.get();
        decoding().erase(pending);
        data = d.data;
        width = d.width;
        height = d.height;
        chn = d.channels;
    } else {
        data = stbi_load (filename.c_str(), &width, &height, &chn, 0);
    }

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

    /* a big one decoded ahead (a level streaming in) goes up in strips over the next frames */
    const bool inStrips = decodedAhead && (size_t) width * height * chn > 8u * 1024u * 1024u;
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  internalFormat[0][chn],
                  width,
                  height,
                  0,
                  pixelFormat[chn],
                  GL_UNSIGNED_BYTE,
                  inStrips ? NULL : data);

    if (!inStrips) glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    /* a little sharper than the screen can show (the anti-aliasing softens a touch); at -0.7 fine
     * print broke up into fragments at a distance instead of blurring */
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.25f);
    /* sharp where it is seen at an angle (the floor into the distance, a wall from the side) */
    if (GLEW_EXT_texture_filter_anisotropic) {
        static float most = -1.0f;
        if (most < 0.0f) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &most);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, most));
    }

    if (inStrips) {
        /* (its one level only until the strips are in: complete meanwhile, not black) */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    if (inStrips) {
        stripUploads().push_back(StripUpload{textureId, data, width, height, chn, 0});
    } else {
        stbi_image_free(data);
    }

    textureCache()[filename] = SharedTexture{textureId, 1};
    m_shared = true;

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
    if (m_shared) {
        auto cached = textureCache().find(filePath);
        if (cached != textureCache().end() && --cached->second.references > 0) {
            return; /* still used by other Texture objects */
        }
        if (cached != textureCache().end()) {
            textureCache().erase(cached);
        }
    }
    /* going up in strips still: not any more (GL gives the id to the next texture made) */
    auto &uploads = stripUploads();
    for (auto up = uploads.begin(); up != uploads.end(); ++up) {
        if (up->id == textureId) {
            stbi_image_free(up->data);
            uploads.erase(up);
            break;
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    if (textureId != 0) glDeleteTextures(1, &textureId);
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

void Texture::use(int etextureUnit)
{
    /* (glEnable(GL_TEXTURE_2D) removed: fixed-function state that shaders
     * ignore, and an invalid enum in core profiles) */
    glActiveTexture(GL_TEXTURE0 + etextureUnit);
    glBindTexture(GL_TEXTURE_2D, textureId);
}
