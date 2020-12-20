#ifndef __RENDERING_OBJECT__
#define __RENDERING_OBJECT__

#include <vector>
#include <string>

#include "Material.h"
#include "Mesh.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

class Mesh;

class RenderingObject {
    public:
        RenderingObject();
        RenderingObject(const std::string &objectFilePath, bool addMaterials = true);

        ~RenderingObject();

        void addMesh(Mesh *mesh);
        void addMaterial(Material *material);

        RenderingObject &operator=(const RenderingObject &otherRenderingObject) = delete;
        RenderingObject(const RenderingObject &otherRenderingObject) = delete;

        RenderingObject &operator=(RenderingObject &&otherRenderingObject);
        RenderingObject(RenderingObject &&otherRenderingObject);

        inline const std::vector<Mesh *> &getMeshes() { return m_meshes; }
        inline const std::vector<Material *> &getMaterials() { return m_materials; }
        inline const std::string &getFilePath() { return m_objectFilePath; }
        inline bool getAddMaterials() { return m_addMaterials; /* if material were added from mtl */ }

        static RenderingObject loadObject(const std::string &filename, bool prepare = true, bool addMaterials = true);
    private:
        std::vector<Mesh *> m_meshes;
        std::vector<Material *> m_materials;
        std::string m_objectFilePath;
        bool m_addMaterials;

        static void processNode(aiNode *node, const aiScene *scene, RenderingObject &renderingObject, bool prepare, bool addMaterials);
        static Mesh *processAssimpMesh(aiMesh *mesh, const aiScene *scene, bool prepare, Material *_material);
};


#endif /* __RENDERING_OBJECT__ */