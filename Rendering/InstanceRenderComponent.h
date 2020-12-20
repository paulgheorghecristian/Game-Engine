#ifndef INSTANCERENDERCOMPONENT_H
#define INSTANCERENDERCOMPONENT_H

#include "RenderingObject.hpp"
#include "Material.h"
#include "Component.h"
#include "Texture.h"
#include "Mesh.h"

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Shader;

class InstanceRenderComponent : public Component
{
    public:
        InstanceRenderComponent(RenderingObject &&renderingObject,
                        const std::vector<glm::vec3>& positionsRotationsScales);
        void input(Input &inputManager);
        void update();
        void render();
        void render (Shader *externShader);
        const unsigned int getFlag() const;

        std::string jsonify() override;

        virtual ~InstanceRenderComponent();

    protected:

    private:
        RenderingObject m_renderingObject;
        unsigned int *m_numberOfTriangles;

        GLuint *vaoHandles;
        enum VBOs {
            VERTEX, INDEX, NUM_VBOS
        };
        GLuint **vboHandles;

        float *matricesBuffer;
        const unsigned int NUM_OF_BYTES_PER_INSTANCE = 16*4;
    	unsigned int m_numOfInstances;

        void fillBufferWithModelMatrices(const std::vector<glm::vec3>&
                                            positionsRotationsScales);
        void createVboHandle(std::size_t idx,
                        const std::vector<Vertex>& vertices,
                        const std::vector<unsigned int>& indices);
    	GLuint createModelMatricesVbo();
        void setAttribsForMatricesVbo(GLuint vboHandle);

        std::vector<glm::vec3> m_positionsRotationsScales;
};

#endif // INSTANCERENDERCOMPONENT_H
