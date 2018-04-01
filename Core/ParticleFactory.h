#ifndef PARTICLEFACTORY_H
#define PARTICLEFACTORY_H

#include "ParticleRenderer.h"

class Display;

class ParticleFactory
{
    public:
        template <typename P>
        static ParticleRenderer<P> *createParticleRenderer(std::string &&jsonPath);

    protected:

    private:
        ParticleFactory() = default;
        ~ParticleFactory() = default;

        static const glm::mat4 &getGlobalProjectionMatrix();
        static Display *getDisplay();
};

template <typename P>
ParticleRenderer<P> *ParticleFactory::createParticleRenderer(std::string &&json) {
    rapidjson::Document doc;
    rapidjson::ParseResult result;
    std::string jsonBody = FileUtils::loadFileInString (json);

    result = doc.Parse (jsonBody.c_str ());
    if (!result) {
        return NULL;
    }

    return new ParticleRenderer<P> (getGlobalProjectionMatrix(),
                                    getDisplay()->getWidth(),
                                    getDisplay()->getHeight(),
                                    doc);
}

#endif // PARTICLEFACTORY_H
