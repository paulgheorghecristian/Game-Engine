#include "InstanceRenderComponent.h"

InstanceRenderComponent::InstanceRenderComponent(const std::string &objModelPath,
                                                Shader *shader,
                                                Texture *texture,
                                                Texture *normalMapTexture,
                                                Texture *roughness,
                                                const Material &material,
                                                const std::vector<glm::vec3>& positionsRotationsScales) :
                                                shader(shader),
                                                texture(texture),
                                                normalMapTexture(normalMapTexture),
                                                roughness(roughness),
                                                material(material),
                                                numOfInstances(positionsRotationsScales.size() / 3),
                                                m_positionsRotationsScales(positionsRotationsScales),
                                                m_objModelPath(objModelPath) {

    bool result = true;
    bool hasTexture = false;
    bool hasNormalMap = false;
    bool hasRoughness = false;

    if (shader != NULL) {
        if (texture != NULL) {
            result &= shader->updateUniform ("textureSampler", texture->getTextureUnit());
            hasTexture = true;
        }

        if (normalMapTexture != NULL) {
            result &= shader->updateUniform ("normalMapSampler", normalMapTexture->getTextureUnit());
            hasNormalMap = true;
        }

        if (roughness != NULL) {
            result &= shader->updateUniform ("roughnessSampler", roughness->getTextureUnit());
            hasRoughness = true;
        }

        result &= shader->updateUniform ("projectionMatrix", (void *) &RenderingMaster::getInstance()->getProjectionMatrix());
        result &= shader->updateUniform ("viewMatrix", (void *) &RenderingMaster::getInstance()->getCamera()->getViewMatrix());
        result &= shader->updateUniform ("material.ambient", (void *) &this->material.getAmbient());
        result &= shader->updateUniform ("material.diffuse", (void *) &material.getDiffuse());
        result &= shader->updateUniform ("material.specular", (void *) &material.getSpecular());
        result &= shader->updateUniform ("material.shininess", material.getShininess());

        result &= shader->updateUniform ("hasTexture", (void *) &hasTexture);
        result &= shader->updateUniform ("hasNormalMap", (void *) &hasNormalMap);
        result &= shader->updateUniform ("hasRoughness", (void *) &hasRoughness);
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    Mesh::loadObjectIntoVectors(objModelPath, vertices, indices);

    numberOfTriangles = indices.size();

    matricesBuffer = new float[numOfInstances * NUM_OF_BYTES_PER_INSTANCE / sizeof(float)];
    fillBufferWithModelMatrices(positionsRotationsScales);

    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    createVboHandle(vertices, indices);
    GLuint modelMatricesVbo = createModelMatricesVbo();

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    //assert (result);
}

void InstanceRenderComponent::input(Input &inputManager) {

}

void InstanceRenderComponent::update() {

}

void InstanceRenderComponent::render(Shader *externShader) {
    bool result = true;

    if (externShader != this->shader) {
        bool hasTexture = false;
        bool hasNormalMap = false;
        bool hasRoughness = false;

        result &= externShader->updateUniform ("material.ambient", (void *) &material.getAmbient());
        result &= externShader->updateUniform ("material.diffuse", (void *) &material.getDiffuse());
        result &= externShader->updateUniform ("material.specular", (void *) &material.getSpecular());
        result &= externShader->updateUniform ("material.shininess", material.getShininess());

        if (texture != NULL) {
            result &= externShader->updateUniform ("textureSampler", texture->getTextureUnit());
            hasTexture = true;
        }

        if (normalMapTexture != NULL) {
            result &= externShader->updateUniform ("normalMapSampler", normalMapTexture->getTextureUnit());
            hasNormalMap = true;
        }

        if (roughness != NULL) {
            result &= externShader->updateUniform ("roughnessSampler", roughness->getTextureUnit());
            hasRoughness = true;
        }

        result &= externShader->updateUniform ("hasTexture", (void *) &hasTexture);
        result &= externShader->updateUniform ("hasNormalMap", (void *) &hasNormalMap);
        result &= externShader->updateUniform ("hasRoughness", (void *) &hasRoughness);
    }

    externShader->bind ();
    if (texture != NULL) {
        texture->use();
    }
    if (normalMapTexture != NULL) {
        normalMapTexture->use();
    }
    if (roughness != NULL) {
        roughness->use();
    }
    glBindVertexArray(vaoHandle);
    glDrawElementsInstanced(GL_TRIANGLES, numberOfTriangles, GL_UNSIGNED_INT, 0, numOfInstances);
    glBindVertexArray(0);
    externShader->unbind();

    //assert (result);
}

void InstanceRenderComponent::render() {
    render(this->shader);
}

const unsigned int InstanceRenderComponent::getFlag() const {
    return Entity::Flags::INSTANCE_RENDERABLE;
}

Shader *InstanceRenderComponent::getShader() {
    return shader;
}

void InstanceRenderComponent::fillBufferWithModelMatrices(const std::vector<glm::vec3>&
                                                positionsRotationsScales) {
    int offset = 0;

    for (unsigned int i = 0; i < positionsRotationsScales.size(); i += 3) {
        const glm::vec3 &position = positionsRotationsScales[i];
        const glm::vec3 &rotation = positionsRotationsScales[i+1];
        const glm::vec3 &scale = positionsRotationsScales[i+2];
        glm::mat4 matrix(1.0f);
        matrix = glm::translate(matrix, position);
        glm::mat4 R = glm::mat4_cast(glm::quat(glm::radians(rotation)));
        matrix = glm::scale(matrix, scale) * R;

        matricesBuffer[offset++] = matrix[0][0];
        matricesBuffer[offset++] = matrix[0][1];
        matricesBuffer[offset++] = matrix[0][2];
        matricesBuffer[offset++] = matrix[0][3];
        matricesBuffer[offset++] = matrix[1][0];
        matricesBuffer[offset++] = matrix[1][1];
        matricesBuffer[offset++] = matrix[1][2];
        matricesBuffer[offset++] = matrix[1][3];
        matricesBuffer[offset++] = matrix[2][0];
        matricesBuffer[offset++] = matrix[2][1];
        matricesBuffer[offset++] = matrix[2][2];
        matricesBuffer[offset++] = matrix[2][3];
        matricesBuffer[offset++] = matrix[3][0];
        matricesBuffer[offset++] = matrix[3][1];
        matricesBuffer[offset++] = matrix[3][2];
        matricesBuffer[offset++] = matrix[3][3];
    }
}

GLuint InstanceRenderComponent::createModelMatricesVbo() {
    GLuint vboHandle;

    glGenBuffers(1, &vboHandle);
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferData(GL_ARRAY_BUFFER, NUM_OF_BYTES_PER_INSTANCE * numOfInstances, (void *) matricesBuffer, GL_STATIC_DRAW);

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, NUM_OF_BYTES_PER_INSTANCE, (void *) 0);
    glVertexAttribDivisor(5, 1);

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, NUM_OF_BYTES_PER_INSTANCE, (void *) (4*4));
    glVertexAttribDivisor(6, 1);

    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, NUM_OF_BYTES_PER_INSTANCE, (void *) (8*4));
    glVertexAttribDivisor(7, 1);

    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, NUM_OF_BYTES_PER_INSTANCE, (void *) (12*4));
    glVertexAttribDivisor(8, 1);

    delete matricesBuffer;
    matricesBuffer = NULL;
    return vboHandle;
}

void InstanceRenderComponent::createVboHandle(const std::vector<Vertex>& vertices,
                                            const std::vector<unsigned int>& indices) {
    glGenBuffers(NUM_VBOS, vboHandles);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[VERTEX]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*vertices.size(), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboHandles[INDEX]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*indices.size(), &indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (2 * sizeof(glm::vec3)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (2 * sizeof(glm::vec3) + sizeof (glm::vec2)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (3 * sizeof(glm::vec3) + sizeof (glm::vec2)));
}

std::string InstanceRenderComponent::jsonify() {
    std::string res("");

    if (m_objModelPath.size() == 0) {
        return res;
    }

    glm::vec3 ambient = material.getAmbient();
    glm::vec3 diffuse = material.getDiffuse();
    glm::vec3 specular = material.getSpecular();
    float shine = material.getShininess();

    res += "\"InstanceRenderComponent\":{";
    res += "\"Mesh\":\"" + m_objModelPath + "\",";
    if (texture != NULL) {
        res += "\"Texture\":\"" + texture->getFilePath() + "\",";
    }
    if (normalMapTexture != NULL) {
        res += "\"NormalMapTexture\":\"" + normalMapTexture->getFilePath() + "\",";
    }
    if (roughness != NULL) {
        res += "\"RoughnessTexture\":\"" + roughness->getFilePath() + "\",";
    }
    res += "\"Material\":{";
    res += "\"ambient\":[" + std::to_string(ambient.x) + ","
            + std::to_string(ambient.y) + "," + std::to_string(ambient.z) + "],";
    res += "\"diffuse\":[" + std::to_string(diffuse.x) + "," 
            + std::to_string(diffuse.y) + "," + std::to_string(diffuse.z) + "],";
    res += "\"specular\":[" + std::to_string(specular.x) + "," 
            + std::to_string(specular.y) + "," + std::to_string(specular.z) + "],";
    res += "\"shininess\":" + std::to_string(shine) + "},";
    res += "\"Transforms\":[";

    for (unsigned int i = 0; i < m_positionsRotationsScales.size(); i+=3) {
        res += "{";
        res += "\"position\":[" + std::to_string(m_positionsRotationsScales[i].x) + ","
            + std::to_string(m_positionsRotationsScales[i].y) + "," + std::to_string(m_positionsRotationsScales[i].z) + "],";
        res += "\"rotation\":[" + std::to_string(m_positionsRotationsScales[i+1].x) + ","
            + std::to_string(m_positionsRotationsScales[i+1].y) + "," + std::to_string(m_positionsRotationsScales[i+1].z) + "],";
        res += "\"scale\":[" + std::to_string(m_positionsRotationsScales[i+2].x) + ","
            + std::to_string(m_positionsRotationsScales[i+2].y) + "," + std::to_string(m_positionsRotationsScales[i+2].z) + "]";
        res += "},";
    }
    res.pop_back();
    res += "]}";

    return res;
}

InstanceRenderComponent::~InstanceRenderComponent() {
    delete shader;
    delete texture;
    delete normalMapTexture;
    delete roughness;
}
