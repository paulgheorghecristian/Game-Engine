#include "ParticleFactory.h"

#include "RenderingMaster.h"

const glm::mat4 &ParticleFactory::getGlobalProjectionMatrix() {
    return RenderingMaster::getInstance()->getProjectionMatrix();
}

Display *ParticleFactory::getDisplay() {
    return RenderingMaster::getInstance()->getDisplay();
}
