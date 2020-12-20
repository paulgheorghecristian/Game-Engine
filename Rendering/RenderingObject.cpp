#include "RenderingObject.hpp"
#include "Mesh.h"
#include "Texture.h"

#include <iostream>

RenderingObject::RenderingObject() {

}

RenderingObject::RenderingObject(const std::string &objectFilePath, bool addMaterials): m_objectFilePath(objectFilePath),
                                                                                    m_addMaterials(addMaterials) {

}

RenderingObject::~RenderingObject() {
    for (unsigned int i = 0; i < m_meshes.size(); i++) {
        delete m_meshes[i];
        m_meshes[i] = NULL;
    }

    for (unsigned int i = 0; i < m_materials.size(); i++) {
        delete m_materials[i];
        m_materials[i] = NULL;
    }
}

void RenderingObject::addMesh(Mesh *mesh) {
    m_meshes.push_back(mesh);
}

void RenderingObject::addMaterial(Material *material) {
    m_materials.push_back(material);
}

RenderingObject &RenderingObject::operator=(RenderingObject &&otherRenderingObject) {
    if (this != &otherRenderingObject) {
        for (unsigned int i = 0; i < m_meshes.size(); i++) {
            delete m_meshes[i];
            m_meshes[i] = NULL;
        }
        for (unsigned int i = 0; i < m_materials.size(); i++) {
            delete m_materials[i];
            m_materials[i] = NULL;
        }
        m_objectFilePath = std::move(otherRenderingObject.m_objectFilePath);
        m_meshes = std::move(otherRenderingObject.m_meshes);
        m_materials = std::move(otherRenderingObject.m_materials);

        m_addMaterials  = otherRenderingObject.m_addMaterials;
    }

    return *this;
}

RenderingObject::RenderingObject(RenderingObject &&otherRenderingObject) {
    m_objectFilePath = std::move(otherRenderingObject.m_objectFilePath);
    m_meshes = std::move(otherRenderingObject.m_meshes);
    m_materials = std::move(otherRenderingObject.m_materials);
    m_addMaterials  = otherRenderingObject.m_addMaterials;
}

RenderingObject RenderingObject::loadObject(const std::string &filename, bool prepare, bool addMaterials) {
    RenderingObject renderingObject(filename, addMaterials);

    if (filename.compare("__circle__") == 0) {
        renderingObject.addMesh(Mesh::getCircle(0, 0, 2.0f, 30));
        if (addMaterials == true) {
            renderingObject.addMaterial(new Material());
        }
        return renderingObject;
    } else if (filename.compare("__rectangle_up__") == 0) {
        renderingObject.addMesh(Mesh::getRectangleYUp());
        if (addMaterials == true) {
            renderingObject.addMaterial(new Material());
        }
        return renderingObject;
    } else if (filename.compare("__dome__") == 0) {
        renderingObject.addMesh(Mesh::getDome(10, 10));
        if (addMaterials == true) {
            renderingObject.addMaterial(new Material());
        }
        return renderingObject;
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                            aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                            aiProcess_GenUVCoords | aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeMeshes |  aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
    {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        std::exit(1);
    }

    processNode(scene->mRootNode, scene, renderingObject, prepare, addMaterials);

    return renderingObject;
}

void RenderingObject::processNode(aiNode *node, const aiScene *scene,
                                    RenderingObject &renderingObject, bool prepare, bool addMaterials) {
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        Material *material = NULL;
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

        if (addMaterials == true) { 
            material = new Material();
        }
        Mesh *_mesh = processAssimpMesh(mesh, scene, prepare, material);

        renderingObject.addMesh(_mesh);
        if (addMaterials == true) {
            renderingObject.addMaterial(material);
        }
    }
    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, renderingObject, prepare, addMaterials);
    }
}

Mesh *RenderingObject::processAssimpMesh(aiMesh *mesh, const aiScene *scene,
                                        bool prepare, Material *_material) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        vertex.positionCoords.x = mesh->mVertices[i].x;
        vertex.positionCoords.y = mesh->mVertices[i].y;
        vertex.positionCoords.z = mesh->mVertices[i].z;

        if (mesh->HasNormals()) {
            vertex.normalCoords.x = mesh->mNormals[i].x;
            vertex.normalCoords.y = mesh->mNormals[i].y;
            vertex.normalCoords.z = mesh->mNormals[i].z;
        }

        if (mesh->mTextureCoords[0]) {
            vertex.textureCoords.x = mesh->mTextureCoords[0][i].x; 
            vertex.textureCoords.y = mesh->mTextureCoords[0][i].y;

            vertex.tangent.x = mesh->mTangents[i].x;
            vertex.tangent.y = mesh->mTangents[i].y;
            vertex.tangent.z = mesh->mTangents[i].z;

            vertex.biTangent.x = mesh->mBitangents[i].x;
            vertex.biTangent.y = mesh->mBitangents[i].y;
            vertex.biTangent.z = mesh->mBitangents[i].z;
        } else {
            vertex.textureCoords = glm::vec2(0.0f, 0.0f);  
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    if (_material != NULL && mesh->mMaterialIndex >= 0) {
        aiColor3D color(0.f, 0.f, 0.f); float shininess;
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        
        material->Get(AI_MATKEY_COLOR_DIFFUSE, color); _material->setDiffuse(glm::vec3(color.r, color.g, color.b));
        material->Get(AI_MATKEY_COLOR_AMBIENT, color); _material->setAmbient(glm::vec3(color.r, color.g, color.b));
        material->Get(AI_MATKEY_COLOR_SPECULAR, color); _material->setSpecular(glm::vec3(color.r, color.g, color.b));
        material->Get(AI_MATKEY_SHININESS, shininess); _material->setShininess(shininess);

        for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_DIFFUSE); i++) {
            aiString str;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
            _material->setDiffuseTexture(new Texture(str.C_Str(), 0));

            break; // one diffuse texture for now
        }

        for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_HEIGHT ); i++) {
            // this seems to be map_Bump from mtl from blender normal map
            aiString str;
            material->GetTexture(aiTextureType_HEIGHT , 0, &str);
            _material->setNormalTexture(new Texture(str.C_Str(), 1));

            break; // one normal texture for now
        }

        for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_SHININESS); i++) {
            aiString str;
            material->GetTexture(aiTextureType_SHININESS, 0, &str);
            _material->setRoughnessTexture(new Texture(str.C_Str(), 2));

            break; // one roughness texture for now
        }
    }

    return new Mesh(vertices, indices, prepare);
}
