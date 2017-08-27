#include "ComponentFactory.h"

ComponentFactory::ComponentFactory() {}

Component *ComponentFactory::createComponent(const rapidjson::Value::ConstMemberIterator& itr) {
    assert (itr->value.IsObject());

    if (strcmp (itr->name.GetString(), "RenderComponent") == 0) {
        Mesh *mesh;
        Shader *shader;
        if (!itr->value.HasMember("Mesh") ||
            !itr->value.HasMember("Shader") ||
            !itr->value.HasMember("Material")) {
            return NULL;
        }

        mesh = Mesh::loadObject (itr->value["Mesh"].GetString());
        shader = new Shader (itr->value["Shader"].GetString());
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
        return new RenderComponent (mesh, shader, material);
    } else {
        return NULL;
    }
}


ComponentFactory::~ComponentFactory() {}
