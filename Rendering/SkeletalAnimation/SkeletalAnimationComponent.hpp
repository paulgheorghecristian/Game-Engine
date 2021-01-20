#ifndef __SKELETAL_ANIM_COMP_HPP__
#define __SKELETAL_ANIM_COMP_HPP__

#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Component.h"
#include "Texture.h"

#include "RenderingObject.hpp"
#include "SkeletalAnimation.hpp"

class SkeletalAnimationComponent : public Component
{
    public:
        SkeletalAnimationComponent(RenderingObject &&renderingObject);
        void input(Input &inputManager);
        void update();
        void render();
        void render (Shader *externShader);
        const unsigned int getFlag() const;
        Shader *getShader();
        std::string jsonify() override;

        void addAnimation(SkeletalAnimation &anim);
        SkeletalAnimation *findAnimation(const std::string &name);
        void playAnimation(SkeletalAnimation &anim, bool loop = false, bool resetToStart = false);
        void stopAnimating();

        inline RenderingObject& getRenderingObject() { return renderingObject; }

        virtual ~SkeletalAnimationComponent();

    protected:

    private:
        RenderingObject renderingObject;
        std::vector<SkeletalAnimation> m_animations;
};

#endif /* __SKELETAL_ANIM_COMP_HPP__ */
