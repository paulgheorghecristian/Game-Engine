#ifndef INSTANCERENDERCOMPONENT_H
#define INSTANCERENDERCOMPONENT_H

#include "RenderingObject.hpp"
#include "MathUtils.h"
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
        /* only the instances that can be seen (each one's bounding sphere); nullptr: all of them */
        void render (Shader *externShader, const ViewPlanes *view, float minimumTop = -1e30f);
        const unsigned int getFlag() const;

        std::string jsonify() override;

        /* "wind": how much the instances sway (trees ~1, 0 = rigid). The
         * bend grows with the height above each instance's base */
        void setWind(float wind);
        float getWind() const { return m_wind; }
        /* number of components that sway (their shadows must be re-rendered) */
        static int s_windyComponents;

        /* "crownShadow": the instances' crown (a tree's leaves) as an ellipsoid in the mesh's own
         * units: centre, horizontal and vertical radius, how dark its shadow is (0..1). Where the
         * sun's shadow map does not reach (far off), the sun light pass casts a soft shadow of it
         * along the light instead */
        void setCrownShadow(const glm::vec3 &centre, float radius, float height, float strength);
        /* two vec4 per instance: (world centre, horizontal radius), (strength, radius / height, 0, 0) */
        void appendCrownShadows(std::vector<glm::vec4> &out) const;

        virtual ~InstanceRenderComponent();

    protected:

    private:
        RenderingObject m_renderingObject;
        float m_wind = 0.0f;
        bool m_hasCrown = false;
        glm::vec3 m_crownCentre = glm::vec3(0.0f);
        float m_crownRadius = 0.0f, m_crownHeight = 0.0f, m_crownStrength = 0.0f;
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
        /* each instance's matrix and bounding sphere (world), and the buffer the GPU draws
         * from: all of them, or (m_partial) only those a pass could see */
        std::vector<glm::mat4> m_matrices;
        std::vector<glm::vec4> m_spheres;
        std::vector<glm::mat4> m_seen;
        GLuint m_matricesVbo = 0;
        bool m_partial = false;
        unsigned int m_drawCount = 0;      /* how many the buffer holds now */
        void upload(const std::vector<glm::mat4> &matrices);
        void draw(Shader *externShader);
};

#endif // INSTANCERENDERCOMPONENT_H
