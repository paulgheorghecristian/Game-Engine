#ifndef INSTANCERENDERCOMPONENT_H
#define INSTANCERENDERCOMPONENT_H

#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Component.h"
#include "RenderingMaster.h"
#include "Texture.h"

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class RenderingMaster;

class InstanceRenderComponent : public Component
{
    public:
        InstanceRenderComponent(const std::string &objModelPath, Shader *shader, Texture *texture,
                        Texture *normalMapTexture, Texture *roughness, const Material &material,
                        const std::vector<glm::vec3>& positionsRotationsScales);
        void input(Input &inputManager);
        void update();
        void render();
        void render (Shader *externShader);
        const unsigned int getFlag() const;
        Shader *getShader();
        virtual ~InstanceRenderComponent();

    protected:

    private:
        Shader *shader;
        Texture *texture, *normalMapTexture, *roughness;
        Material material;
        unsigned int numberOfTriangles;

        GLuint vaoHandle;
        enum VBOs {
            VERTEX, INDEX, NUM_VBOS
        };
        GLuint vboHandles[NUM_VBOS];
        float *matricesBuffer;
        const unsigned int NUM_OF_BYTES_PER_INSTANCE = 16*4;
    	unsigned int numOfInstances;

        void fillBufferWithModelMatrices(const std::vector<glm::vec3>&
                                            positionsRotationsScales);
        void createVboHandle(const std::vector<Vertex>& vertices,
                        const std::vector<unsigned int>& indices);
    	GLuint createModelMatricesVbo();
};

#endif // INSTANCERENDERCOMPONENT_H
