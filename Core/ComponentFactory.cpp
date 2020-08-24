#include "ComponentFactory.h"

ComponentFactory::ComponentFactory() {}

Component *ComponentFactory::createComponent(const rapidjson::Value::ConstMemberIterator& itr) {
    assert (itr->value.IsObject());

    if (strcmp (itr->name.GetString(), "RenderComponent") == 0) {
        Mesh *mesh;
        Shader *shader;
        Texture *texture = NULL;
        Texture *normalMapTexture = NULL;
        Texture *roughness = NULL;
        if (!itr->value.HasMember("Mesh") ||
            !itr->value.HasMember("Shader") ||
            !itr->value.HasMember("Material")) {
            return NULL;
        }

        mesh = Mesh::loadObject (itr->value["Mesh"].GetString());
        shader = new Shader ();
        shader->construct (itr->value["Shader"].GetString());
        if (itr->value.HasMember("Texture")) {
            texture = new Texture(itr->value["Texture"].GetString(), 0);
        }
        if (itr->value.HasMember("NormalMapTexture")) {
            normalMapTexture = new Texture (itr->value["NormalMapTexture"].GetString(), 1);
        }
        if (itr->value.HasMember("RoughnessTexture")) {
            roughness = new Texture(itr->value["RoughnessTexture"].GetString(), 2);
        }
        Material material(glm::vec3(itr->value["Material"]["ambient"].GetArray()[0].GetFloat(),
                                      itr->value["Material"]["ambient"].GetArray()[1].GetFloat(),
                                      itr->value["Material"]["ambient"].GetArray()[2].GetFloat()),
                            glm::vec3(itr->value["Material"]["diffuse"].GetArray()[0].GetFloat(),
                                      itr->value["Material"]["diffuse"].GetArray()[1].GetFloat(),
                                      itr->value["Material"]["diffuse"].GetArray()[2].GetFloat()),
                            glm::vec3(itr->value["Material"]["specular"].GetArray()[0].GetFloat(),
                                      itr->value["Material"]["specular"].GetArray()[1].GetFloat(),
                                      itr->value["Material"]["specular"].GetArray()[2].GetFloat()),
                            itr->value["Material"]["shininess"].GetFloat());
        return new RenderComponent (mesh, shader, texture, normalMapTexture, roughness, material);
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
        } else if (strcmp (bodyType, "simplified mesh") == 0) {
            //TODO NOT YET SUPPORTED
            const char *meshPath = itr->value["BoundingBodyType"].GetArray()[1].GetString();

            assert (false);
        }

        scale = glm::vec3 (itr->value["scale"].GetArray()[0].GetFloat(),
                           itr->value["scale"].GetArray()[1].GetFloat(),
                           itr->value["scale"].GetArray()[2].GetFloat());
        mass = itr->value["mass"].GetFloat();

        return new PhysicsComponent(type, scale, mass);
    } else if (strcmp (itr->name.GetString(), "GrabComponent") == 0) {
        return new GrabComponent(itr->value["radius"].GetFloat());
    } else {
        return NULL;
    }
}

ComponentFactory::~ComponentFactory() {}
