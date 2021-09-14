#include "ComponentFactory.h"

#include "RenderingObject.hpp"

std::function<void(bool start, Entity *entity)> ComponentFactory::y_90_rotation =
[rot=float(0.0f)](bool start, Entity *entity) mutable {
    static float speed = 1.5f;

    if (start == true && rot <= 0.0f)
        rot = 90.0f;

    if (rot > 0.0f) {
        entity->getTransform().addRotation(0, speed, 0, true);
        rot -= speed;
    }
};

ComponentFactory::ComponentFactory() {}

Component *ComponentFactory::createComponent(const rapidjson::Value::ConstMemberIterator& itr) {
    assert (itr->value.IsObject());

    if (strcmp (itr->name.GetString(), "RenderComponent") == 0) {
        RenderingObject renderingObject;
        Texture *texture = NULL;
        Texture *normalMapTexture = NULL;
        Texture *roughness = NULL;

        Material *material = NULL;

        if (!itr->value.HasMember("Mesh")) {
            return NULL;
        }

        if (itr->value.HasMember("Material")) {
            material = new Material();

            material->setAmbient(glm::vec3(itr->value["Material"]["ambient"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["ambient"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["ambient"].GetArray()[2].GetFloat()));

            material->setDiffuse(glm::vec3(itr->value["Material"]["diffuse"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["diffuse"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["diffuse"].GetArray()[2].GetFloat()));

            material->setSpecular(glm::vec3(itr->value["Material"]["specular"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["specular"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["specular"].GetArray()[2].GetFloat()));

            material->setShininess(itr->value["Material"]["shininess"].GetFloat());
            if (itr->value["Material"].HasMember("normalMapStrength")) {
                material->setNormalMapStrength(itr->value["Material"]["normalMapStrength"].GetFloat());
            }
            if (itr->value["Material"].HasMember("Texture")) {
                texture = new Texture(itr->value["Material"]["Texture"].GetString(), 0);
                material->setDiffuseTexture(texture);

            }
            if (itr->value["Material"].HasMember("NormalMapTexture")) {
                normalMapTexture = new Texture (itr->value["Material"]["NormalMapTexture"].GetString(), 1);
                material->setNormalTexture(normalMapTexture);
            }
            if (itr->value["Material"].HasMember("RoughnessTexture")) {
                roughness = new Texture(itr->value["Material"]["RoughnessTexture"].GetString(), 2);
                material->setRoughnessTexture(roughness);
            }
        }

        renderingObject = RenderingObject::loadObject(itr->value["Mesh"].GetString(), true, material == NULL);
        if (material != NULL) {
            renderingObject.addMaterial(material);
        }
        return new RenderComponent(std::move(renderingObject));
    } else if (strcmp (itr->name.GetString(), "TempAnimationComponent") == 0) {
        glm::vec3 addPosition(0), addRotation(0), addScale(0);
        if (itr->value.HasMember("addPosition")) {
            addPosition = glm::vec3 (itr->value["addPosition"].GetArray()[0].GetFloat(),
                                     itr->value["addPosition"].GetArray()[1].GetFloat(),
                                     itr->value["addPosition"].GetArray()[2].GetFloat());
        }
        if (itr->value.HasMember("addRotation")) {
            addRotation = glm::vec3 (itr->value["addRotation"].GetArray()[0].GetFloat(),
                                     itr->value["addRotation"].GetArray()[1].GetFloat(),
                                     itr->value["addRotation"].GetArray()[2].GetFloat());
        }
        if (itr->value.HasMember("addScale")) {
            addScale = glm::vec3 (itr->value["addScale"].GetArray()[0].GetFloat(),
                                  itr->value["addScale"].GetArray()[1].GetFloat(),
                                  itr->value["addScale"].GetArray()[2].GetFloat());
        }

        return new TempAnimationComponent (addPosition, addRotation, addScale);
    } else if (strcmp (itr->name.GetString(), "PhysicsComponent") == 0) {
        PhysicsComponent::BoundingBodyType type;
        glm::vec3 scale(1.0f);
        float mass;
        const char *bodyType;
        bool shouldUpdate = true;

        if (!itr->value.HasMember ("BoundingBodyType") ||
            !itr->value.HasMember ("scale") ||
            !itr->value.HasMember ("mass")) {
            return NULL;
        }

        bodyType = itr->value["BoundingBodyType"].GetArray()[0].GetString();
        if (strcmp (bodyType, "cube") == 0) {
            type = PhysicsComponent::BoundingBodyType::CUBE;
        } else if (strcmp (bodyType, "sphere") == 0) {
            type = PhysicsComponent::BoundingBodyType::SPHERE;
        } else if (strcmp (bodyType, "capsule") == 0) {
            type = PhysicsComponent::BoundingBodyType::CAPSULE;
        } else if (strcmp (bodyType, "simplified mesh") == 0) {
            //TODO NOT YET SUPPORTED
            const char *meshPath = itr->value["BoundingBodyType"].GetArray()[1].GetString();

            assert (false);
        }

        scale = glm::vec3 (itr->value["scale"].GetArray()[0].GetFloat(),
                           itr->value["scale"].GetArray()[1].GetFloat(),
                           itr->value["scale"].GetArray()[2].GetFloat());
        mass = itr->value["mass"].GetFloat();

        if (itr->value.HasMember ("shouldUpdate")) {
            shouldUpdate = itr->value["shouldUpdate"].GetBool();
        }

        return new PhysicsComponent(type, scale, mass, shouldUpdate);
    } else if (strcmp (itr->name.GetString(), "GrabComponent") == 0) {

        return new GrabComponent(itr->value["radius"].GetFloat());
    } else if (strcmp(itr->name.GetString(), "ActionComponent") == 0) {
        if (strcmp(itr->value["action"].GetString(), "y90rotation") == 0) {
            return new ActionComponent( itr->value["radius"].GetFloat(), ComponentFactory::y_90_rotation, "y90rotation");
        } else {
            return NULL;
        }
    } else if (strcmp(itr->name.GetString(), "BillboardComponent") == 0) {
        return new BillboardComponent();
    } else if (strcmp(itr->name.GetString(), "InstanceRenderComponent") == 0) {
        RenderingObject renderingObject;
        Texture *texture = NULL;
        Texture *normalMapTexture = NULL;
        Texture *roughness = NULL;
        Material *material = NULL;
        std::vector<glm::vec3> positionsRotationsScales = {};

        if (!itr->value.HasMember("Mesh")) {
            return NULL;
        }

        if (itr->value.HasMember("Material")) {
            material = new Material();

            material->setAmbient(glm::vec3(itr->value["Material"]["ambient"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["ambient"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["ambient"].GetArray()[2].GetFloat()));

            material->setDiffuse(glm::vec3(itr->value["Material"]["diffuse"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["diffuse"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["diffuse"].GetArray()[2].GetFloat()));

            material->setSpecular(glm::vec3(itr->value["Material"]["specular"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["specular"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["specular"].GetArray()[2].GetFloat()));

            material->setShininess(itr->value["Material"]["shininess"].GetFloat());
            if (itr->value["Material"].HasMember("normalMapStrength")) {
                material->setNormalMapStrength(itr->value["Material"]["normalMapStrength"].GetFloat());
            }

            if (itr->value["Material"].HasMember("Texture")) {
                texture = new Texture(itr->value["Material"]["Texture"].GetString(), 0);
                material->setDiffuseTexture(texture);

            }
            if (itr->value["Material"].HasMember("NormalMapTexture")) {
                normalMapTexture = new Texture (itr->value["Material"]["NormalMapTexture"].GetString(), 1);
                material->setNormalTexture(normalMapTexture);
            }
            if (itr->value["Material"].HasMember("RoughnessTexture")) {
                roughness = new Texture(itr->value["Material"]["RoughnessTexture"].GetString(), 2);
                material->setRoughnessTexture(roughness);
            }
        }

        renderingObject = RenderingObject::loadObject(itr->value["Mesh"].GetString(), false, material == NULL);
        if (material != NULL) {
            renderingObject.addMaterial(material);
        }
        if (itr->value.HasMember("Transforms")) {
            for (auto const &posRotScale : itr->value["Transforms"].GetArray()) {
                glm::vec3 position, rotation, scale;
                position.x = posRotScale["position"].GetArray()[0].GetFloat();
                position.y = posRotScale["position"].GetArray()[1].GetFloat();
                position.z = posRotScale["position"].GetArray()[2].GetFloat();

                rotation.x = posRotScale["rotation"].GetArray()[0].GetFloat();
                rotation.y = posRotScale["rotation"].GetArray()[1].GetFloat();
                rotation.z = posRotScale["rotation"].GetArray()[2].GetFloat();

                scale.x = posRotScale["scale"].GetArray()[0].GetFloat();
                scale.y = posRotScale["scale"].GetArray()[1].GetFloat();
                scale.z = posRotScale["scale"].GetArray()[2].GetFloat();

                positionsRotationsScales.push_back(position);
                positionsRotationsScales.push_back(rotation);
                positionsRotationsScales.push_back(scale);
            }
        }
        return new InstanceRenderComponent(std::move(renderingObject), positionsRotationsScales);
    } if (strcmp (itr->name.GetString(), "SkeletalAnimationComponent") == 0) {
        RenderingObject renderingObject;
        Texture *texture = NULL;
        Texture *normalMapTexture = NULL;
        Texture *roughness = NULL;

        Material *material = NULL;

        if (!itr->value.HasMember("Mesh")) {
            return NULL;
        }

        if (itr->value.HasMember("Material")) {
            material = new Material();

            material->setAmbient(glm::vec3(itr->value["Material"]["ambient"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["ambient"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["ambient"].GetArray()[2].GetFloat()));

            material->setDiffuse(glm::vec3(itr->value["Material"]["diffuse"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["diffuse"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["diffuse"].GetArray()[2].GetFloat()));

            material->setSpecular(glm::vec3(itr->value["Material"]["specular"].GetArray()[0].GetFloat(),
                                        itr->value["Material"]["specular"].GetArray()[1].GetFloat(),
                                        itr->value["Material"]["specular"].GetArray()[2].GetFloat()));

            material->setShininess(itr->value["Material"]["shininess"].GetFloat());
            if (itr->value["Material"].HasMember("normalMapStrength")) {
                material->setNormalMapStrength(itr->value["Material"]["normalMapStrength"].GetFloat());
            }
            if (itr->value["Material"].HasMember("Texture")) {
                texture = new Texture(itr->value["Material"]["Texture"].GetString(), 0);
                material->setDiffuseTexture(texture);

            }
            if (itr->value["Material"].HasMember("NormalMapTexture")) {
                normalMapTexture = new Texture (itr->value["Material"]["NormalMapTexture"].GetString(), 1);
                material->setNormalTexture(normalMapTexture);
            }
            if (itr->value["Material"].HasMember("RoughnessTexture")) {
                roughness = new Texture(itr->value["Material"]["RoughnessTexture"].GetString(), 2);
                material->setRoughnessTexture(roughness);
            }
        }

        renderingObject = RenderingObject::loadObject(itr->value["Mesh"].GetString(), true, material == NULL);
        if (material != NULL) {
            renderingObject.addMaterial(material);
        }
        return new SkeletalAnimationComponent(std::move(renderingObject));
    } else {
        return NULL;
    }
}

ComponentFactory::~ComponentFactory() {}
