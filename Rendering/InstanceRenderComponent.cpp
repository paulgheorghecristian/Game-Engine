#include "InstanceRenderComponent.h"

#include "Shader.h"

InstanceRenderComponent::InstanceRenderComponent(RenderingObject &&renderingObject,
                                                const std::vector<glm::vec3>& positionsRotationsScales) :
                                                m_renderingObject(std::move(renderingObject)),
                                                m_numOfInstances(positionsRotationsScales.size() / 3),
                                                m_positionsRotationsScales(positionsRotationsScales) {

    const std::size_t numMeshes = m_renderingObject.getMeshes().size();

    m_numberOfTriangles = new unsigned int[numMeshes];
    vaoHandles = new GLuint[numMeshes];

    vboHandles = new GLuint *[numMeshes];
    for (std::size_t i = 0; i < numMeshes; i++) {
        vboHandles[i] = new GLuint[NUM_VBOS];
    }

    matricesBuffer = new float[m_numOfInstances * NUM_OF_BYTES_PER_INSTANCE / sizeof(float)];
    fillBufferWithModelMatrices(positionsRotationsScales);
    GLuint modelMatricesVbo = createModelMatricesVbo();

    glGenVertexArrays(numMeshes, vaoHandles);

    for (std::size_t i = 0; i < numMeshes; i++) {
        glBindVertexArray(vaoHandles[i]);

        m_numberOfTriangles[i] = (m_renderingObject.getMeshes()[i])->getIndices().size();
        createVboHandle(i,
                        (m_renderingObject.getMeshes()[i])->getVertices(),
                        (m_renderingObject.getMeshes()[i])->getIndices());

        setAttribsForMatricesVbo(modelMatricesVbo);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void InstanceRenderComponent::input(Input &inputManager) {

}

void InstanceRenderComponent::update() {

}

void InstanceRenderComponent::render(Shader *externShader) {
    bool result = true;

    const std::vector<Mesh *> &m_meshes = m_renderingObject.getMeshes();
    const std::vector<Material *> &m_materials = m_renderingObject.getMaterials();

    for (unsigned int i = 0; i < m_meshes.size(); i++) {
        bool hasTexture = false;
        bool hasNormalMap = false;
        bool hasRoughness = false;

        result &= externShader->updateUniform ("material.ambient", (void *) &m_materials[i]->getAmbient());
        result &= externShader->updateUniform ("material.diffuse", (void *) &m_materials[i]->getDiffuse());
        result &= externShader->updateUniform ("material.specular", (void *) &m_materials[i]->getSpecular());
        result &= externShader->updateUniform ("material.shininess", m_materials[i]->getShininess());

        if (m_materials[i]->getDiffuseTexture() != NULL) {
            result &= externShader->updateUniform ("textureSampler", m_materials[i]->getDiffuseTexture()->getTextureUnit());
            hasTexture = true;
        }

        if (m_materials[i]->getNormalTexture() != NULL) {
            result &= externShader->updateUniform ("normalMapSampler", m_materials[i]->getNormalTexture()->getTextureUnit());
            hasNormalMap = true;
        }

        if (m_materials[i]->getRoughnessTexture() != NULL) {
            result &= externShader->updateUniform ("roughnessSampler", m_materials[i]->getRoughnessTexture()->getTextureUnit());
            hasRoughness = true;
        }

        result &= externShader->updateUniform ("hasTexture", (void *) &hasTexture);
        result &= externShader->updateUniform ("hasNormalMap", (void *) &hasNormalMap);
        result &= externShader->updateUniform ("hasRoughness", (void *) &hasRoughness);

        if (m_materials[i]->getDiffuseTexture() != NULL) {
            m_materials[i]->getDiffuseTexture()->use();
        }
        if (m_materials[i]->getNormalTexture() != NULL) {
            m_materials[i]->getNormalTexture()->use();
        }
        if (m_materials[i]->getRoughnessTexture() != NULL) {
            m_materials[i]->getRoughnessTexture()->use();
        }
        externShader->bind();
        glBindVertexArray(vaoHandles[i]);
        glDrawElementsInstanced(GL_TRIANGLES, m_numberOfTriangles[i], GL_UNSIGNED_INT, 0, m_numOfInstances);
        glBindVertexArray(0);

        bool addMat = m_renderingObject.getAddMaterials();
        if (addMat == false) {
            /* only 1 mesh is supported when not adding from mtl */
            break;
        }
    }    
    externShader->unbind();

    //assert (result);
}

void InstanceRenderComponent::render() {
}

const unsigned int InstanceRenderComponent::getFlag() const {
    return Entity::Flags::INSTANCE_RENDERABLE;
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
    glBufferData(GL_ARRAY_BUFFER, NUM_OF_BYTES_PER_INSTANCE * m_numOfInstances, (void *) matricesBuffer, GL_STATIC_DRAW);

    setAttribsForMatricesVbo(vboHandle);

    delete matricesBuffer;
    matricesBuffer = NULL;
    return vboHandle;
}

void InstanceRenderComponent::setAttribsForMatricesVbo(GLuint vboHandle) {
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);

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
}

void InstanceRenderComponent::createVboHandle(std::size_t idx,
                                            const std::vector<Vertex>& vertices,
                                            const std::vector<unsigned int>& indices) {
    glGenBuffers(NUM_VBOS, vboHandles[idx]);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[idx][VERTEX]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboHandles[idx][INDEX]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), &indices[0], GL_STATIC_DRAW);

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

    if (m_renderingObject.getFilePath().size() == 0) {
        return res;
    }

    bool addMat = m_renderingObject.getAddMaterials();

    res += "\"InstanceRenderComponent\":{";
    res += "\"Mesh\":\"" + m_renderingObject.getFilePath() + "\"";   

    if (addMat == false && m_renderingObject.getMaterials().size() > 0) {
        res += ",\"Material\":{";
        if ((m_renderingObject.getMaterials()[0])->getDiffuseTexture() != NULL) {
          res += "\"Texture\":\"" + (m_renderingObject.getMaterials()[0])->getDiffuseTexture()->getFilePath() + "\",";
        }
        if ((m_renderingObject.getMaterials()[0])->getNormalTexture() != NULL) {
            res += "\"NormalMapTexture\":\"" + (m_renderingObject.getMaterials()[0])->getNormalTexture()->getFilePath() + "\",";
        }
        if ((m_renderingObject.getMaterials()[0])->getRoughnessTexture() != NULL) {
            res += "\"RoughnessTexture\":\"" + (m_renderingObject.getMaterials()[0])->getRoughnessTexture()->getFilePath() + "\",";
        }
        glm::vec3 ambient = (m_renderingObject.getMaterials()[0])->getAmbient();
        glm::vec3 diffuse = (m_renderingObject.getMaterials()[0])->getDiffuse();
        glm::vec3 specular = (m_renderingObject.getMaterials()[0])->getSpecular();
        float shine = (m_renderingObject.getMaterials()[0])->getShininess();

        res += "\"ambient\":[" + std::to_string(ambient.x) + ","
                + std::to_string(ambient.y) + "," + std::to_string(ambient.z) + "],";
        res += "\"diffuse\":[" + std::to_string(diffuse.x) + "," 
                + std::to_string(diffuse.y) + "," + std::to_string(diffuse.z) + "],";
        res += "\"specular\":[" + std::to_string(specular.x) + "," 
                + std::to_string(specular.y) + "," + std::to_string(specular.z) + "],";
        res += "\"shininess\":" + std::to_string(shine) + "}";
    }

    res += ",\"Transforms\":[";

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
        delete[] m_numberOfTriangles;

        glDeleteVertexArrays(m_renderingObject.getMeshes().size(), vaoHandles);
        delete[] vaoHandles;
        for (std::size_t i = 0; i < m_renderingObject.getMeshes().size(); i++) {
            glDeleteBuffers(NUM_VBOS, vboHandles[i]);
            delete[] vboHandles[i];
        }
        delete[] vboHandles;
}
