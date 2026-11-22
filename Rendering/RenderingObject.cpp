#include "RenderingObject.hpp"
#include "Mesh.h"
#include "Texture.h"
#include "AnimationLoader.hpp"
#include "Skeleton.hpp"

#include <iostream>
#include <limits>
#include <unordered_map>

RenderingObject::RenderingObject() {
    m_skeleton = NULL;
}

RenderingObject::RenderingObject(const std::string &objectFilePath, bool addMaterials): m_objectFilePath(objectFilePath),
                                                                                    m_addMaterials(addMaterials) {
   m_skeleton = NULL;
}

RenderingObject::~RenderingObject() {
    for (unsigned int i = 0; i < m_meshes.size(); i++) {
        Mesh::release(m_meshes[i]);
        m_meshes[i] = NULL;
    }

    for (unsigned int i = 0; i < m_materials.size(); i++) {
        delete m_materials[i];
        m_materials[i] = NULL;
    }

    delete m_skeleton;
    m_skeleton = NULL;
}

void RenderingObject::addMesh(Mesh *mesh) {
    m_meshes.push_back(mesh);
    m_boundsValid = false;
}

void RenderingObject::getBoundingSphere(glm::vec3 &center, float &radius) {
    if (!m_boundsValid) {
        glm::vec3 mn(std::numeric_limits<float>::max()), mx(-std::numeric_limits<float>::max());
        bool any = false;
        for (Mesh *mesh : m_meshes) {
            for (const Vertex &v : mesh->getVertices()) {
                mn = glm::min(mn, v.positionCoords);
                mx = glm::max(mx, v.positionCoords);
                any = true;
            }
        }
        m_boundsCenter = any ? (mn + mx) * 0.5f : glm::vec3(0.0f);
        m_boundsRadius = 0.0f;
        for (Mesh *mesh : m_meshes) {
            for (const Vertex &v : mesh->getVertices()) {
                m_boundsRadius = glm::max(m_boundsRadius, glm::length(v.positionCoords - m_boundsCenter));
            }
        }
        m_boundsValid = true;
    }
    center = m_boundsCenter;
    radius = m_boundsRadius;
}

void RenderingObject::addMaterial(Material *material) {
    m_materials.push_back(material);
}

RenderingObject &RenderingObject::operator=(RenderingObject &&otherRenderingObject) {
    if (this != &otherRenderingObject) {
        for (unsigned int i = 0; i < m_meshes.size(); i++) {
            Mesh::release(m_meshes[i]);
            m_meshes[i] = NULL;
        }
        for (unsigned int i = 0; i < m_materials.size(); i++) {
            delete m_materials[i];
            m_materials[i] = NULL;
        }
        if (m_skeleton != NULL) {
            delete m_skeleton;
            m_skeleton = NULL;
        }

        m_objectFilePath = std::move(otherRenderingObject.m_objectFilePath);
        m_meshes = std::move(otherRenderingObject.m_meshes);
        m_materials = std::move(otherRenderingObject.m_materials);

        m_addMaterials  = otherRenderingObject.m_addMaterials;
        m_skeleton = otherRenderingObject.m_skeleton;
        otherRenderingObject.m_skeleton = NULL;
    }

    return *this;
}

RenderingObject::RenderingObject(RenderingObject &&otherRenderingObject) {
    m_objectFilePath = std::move(otherRenderingObject.m_objectFilePath);
    m_meshes = std::move(otherRenderingObject.m_meshes);
    m_materials = std::move(otherRenderingObject.m_materials);
    m_addMaterials  = otherRenderingObject.m_addMaterials;

    m_skeleton = otherRenderingObject.m_skeleton;
    otherRenderingObject.m_skeleton = NULL;
}

/* Model files are parsed and uploaded once. The cache keeps a prototype per
 * (file, prepare, addMaterials); later loads share its meshes and get their
 * own copies of its materials (entities may change those independently).
 * Skinned models are not cached: their meshes carry per-instance bone data. */
static std::unordered_map<std::string, RenderingObject *> &objectCache() {
    static std::unordered_map<std::string, RenderingObject *> cache;
    return cache;
}

static RenderingObject *shareObject(RenderingObject &source, const std::string &filename, bool addMaterials) {
    RenderingObject *copy = new RenderingObject(filename, addMaterials);
    for (Mesh *mesh : source.getMeshes()) {
        mesh->retain();
        copy->addMesh(mesh);
    }
    for (Material *material : source.getMaterials()) {
        copy->addMaterial(material->clone());
    }
    return copy;
}

RenderingObject RenderingObject::loadObject(const std::string &filename, bool prepare, bool addMaterials) {
    const bool cacheable = filename.compare(0, 2, "__") != 0;
    const std::string cacheKey = filename + (prepare ? "|gpu" : "|cpu") + (addMaterials ? "|mtl" : "|nomtl");
    if (cacheable) {
        auto cached = objectCache().find(cacheKey);
        if (cached != objectCache().end()) {
            RenderingObject *copy = shareObject(*cached->second, filename, addMaterials);
            RenderingObject result(std::move(*copy));
            delete copy;
            return result;
        }
    }

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
    aiScene *scene = NULL;
    (void) importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                            aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                            aiProcess_GenUVCoords | aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeMeshes |  aiProcess_OptimizeGraph);
    scene = importer.GetOrphanedScene();

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
    {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        std::exit(1);
    }

    AnimationLoader::init(scene);
    AnimationLoader::getInstance()->loadNodesAnim();
    AnimationLoader::getInstance()->recursiveLoadNodes(scene->mRootNode);

    // load meshes and bones
    processNode(scene->mRootNode, scene, renderingObject, prepare, addMaterials);
    AnimationLoader::getInstance()->loadBoneParents();

    // create skeleton from bones
    if (AnimationLoader::getInstance()->getBones().size() > 0) {
        Skeleton *skeleton = new Skeleton(scene,
                                        AnimationLoader::getInstance()->getBones(),
                                        AnimationLoader::getInstance()->getGlobalInverserTransform());
        renderingObject.setSkeleton(skeleton);
    }

    AnimationLoader::destroy();

    if (cacheable && renderingObject.getSkeleton() == NULL) {
        objectCache()[cacheKey] = shareObject(renderingObject, filename, addMaterials);
    }

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
    const int WEIGHTS_PER_VERTEX = 4;

    int boneArraysSize = mesh->mNumVertices*WEIGHTS_PER_VERTEX;
    std::vector<unsigned int> boneIDs;
    std::vector<float> boneWeights;

    boneIDs.resize(boneArraysSize);
    boneWeights.resize(boneArraysSize);

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

            bool isBlackAlpha = false;
            if (aiReturn_SUCCESS == material->Get(AI_MATKEY_ISBLACKALPHA, isBlackAlpha)) {
                _material->setIsBlackAlpha(isBlackAlpha);
            }

            bool disableCulling = false;
            if (aiReturn_SUCCESS == material->Get(AI_MATKEY_DISABLECULLING, disableCulling)) {
                _material->setDisableCulling(disableCulling);
            }

            break; // one diffuse texture for now
        }

        if (material->GetTextureCount(aiTextureType_NORMALS) > 0) {
            for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_NORMALS); i++) {
                // this seems to be map_Bump from mtl from blender normal map
                aiString str;
                material->GetTexture(aiTextureType_NORMALS , 0, &str);
                _material->setNormalTexture(new Texture(str.C_Str(), 1));

                float bumpScaling = 1.0f;
                if (aiReturn_SUCCESS == material->Get(AI_MATKEY_NORMALMAPSTRENGTH, bumpScaling)) {
                    _material->setNormalMapStrength(bumpScaling);
                }

                break; // one normal texture for now
            }
        } else {
            for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_HEIGHT ); i++) {
                // this seems to be map_Bump from mtl from blender normal map
                aiString str;
                material->GetTexture(aiTextureType_HEIGHT , 0, &str);
                _material->setNormalTexture(new Texture(str.C_Str(), 1));

                float bumpScaling = 1.0f;
                if (aiReturn_SUCCESS == material->Get(AI_MATKEY_NORMALMAPSTRENGTH, bumpScaling)) {
                    _material->setNormalMapStrength(bumpScaling);
                }

                break; // one normal texture for now
            }
        }

        for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_SPECULAR); i++) {
            aiString str;
            material->GetTexture(aiTextureType_SPECULAR, 0, &str);
            _material->setRoughnessTexture(new Texture(str.C_Str(), 2));

            break; // one roughness texture for now
        }
    }

    Mesh *_mesh = new Mesh(prepare, true);

    SkeletalBone *bone = NULL;

    std::size_t offset = AnimationLoader::getInstance()->getBones().size();

    for (unsigned int i = 0; i < mesh->mNumBones; i++) {
        aiBone *aiBone = mesh->mBones[i];

        const std::string bName = aiBone->mName.data;
        const glm::mat4 bMat = glm::transpose(AnimationLoader::AiToGLMMat4(aiBone->mOffsetMatrix));

        std::cout<<"Bone "<<i<<" "<<bName<<std::endl;

        bone = new SkeletalBone(_mesh, i+offset, bName, bMat);

        bone->m_node = AnimationLoader::getInstance()->FindAiNode(bName);
        bone->m_animNode = AnimationLoader::getInstance()->FindAiNodeAnim(bName);

        if (bone->m_animNode == NULL)
            std::cout<<"No Animations were found for "+bName<<std::endl;

        for (unsigned int j = 0; j < aiBone->mNumWeights; j++) {
            aiVertexWeight weight = aiBone->mWeights[j];
            unsigned int vertexStart = weight.mVertexId * WEIGHTS_PER_VERTEX;

            for (unsigned int k = 0; k < WEIGHTS_PER_VERTEX; k++) {
                if (boneWeights.at(vertexStart+k) == 0) {
                    boneWeights.at(vertexStart+k) = weight.mWeight;
                    boneIDs.at(vertexStart+k) = i;

                    vertices.at(weight.mVertexId).bonesIdx[k] = i+offset;
                    vertices.at(weight.mVertexId).bonesWeights[k] = weight.mWeight;

                    break;
                }
            }
        }

        AnimationLoader::getInstance()->addBone(bone);
    }

    _mesh->update(vertices, indices, false);
    return _mesh;
}
