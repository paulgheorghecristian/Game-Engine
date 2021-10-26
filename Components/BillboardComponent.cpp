#include "BillboardComponent.h"

#include "RenderingMaster.h"
#include "Common.h"

// Component that keeps entity facing the camera
BillboardComponent::BillboardComponent() {

}

void BillboardComponent::input(Input &inputManager) {

}

void BillboardComponent::update() {
    Transform &transform = _entity->getTransform();
    const glm::mat4 &viewMatrix = camera->getViewMatrix();
    const glm::vec3 &position = transform.getPosition();
    const glm::vec3 &scale = transform.getScale();

    glm::mat4 modelMatrix = glm::mat4(1.0f);

    for (int i = 0; i < 3; i++){
		for (int j = 0; j < 3; j++){
			modelMatrix[i][j] = viewMatrix[j][i];
		}
	}

	modelMatrix[3][0] = position[0];
	modelMatrix[3][1] = position[1];
	modelMatrix[3][2] = position[2];

	transform.setModelMatrix(modelMatrix * glm::scale(glm::mat4(1.0), scale));
}

void BillboardComponent::render() {

}

void BillboardComponent::init() {
    assert (_entity != NULL);

    camera = RenderingMaster::getInstance()->getCamera();
}

const unsigned int BillboardComponent::getFlag() const {
    return Entity::Flags::BILL;
}

std::string BillboardComponent::jsonify() {
    std::string res("");

    res += "\"BillboardComponent\":{}";

    return res;
}

BillboardComponent::~BillboardComponent()
{

}
