#ifndef __AIPLAYERFOLLOWERCOMPONENT_HPP__
#define __AIPLAYERFOLLOWERCOMPONENT_HPP__

#include "Component.h"
#include "PhysicsMaster.h"
#include "Player.h"

class AIPlayerFollowerComponent : public Component
{
    public:
        AIPlayerFollowerComponent(Player *player);
        void input(Input &inputManager);
        void update();
        void render() {};
        void init();
        const unsigned int getFlag() const;

        std::string jsonify() override;

    protected:

    private:
        Player *player;
        std::vector<std::size_t> lastFoundPathToPlayer;
        std::vector<glm::vec3> lastFoundPosToPlayer;
        std::size_t m_currentIdx, m_currTargetIdx, m_Idx;

        glm::vec3 m_dirToCurrTarget, m_dirToNextTarget, m_lastPosition;
        bool m_interpolate, isSameIdx;
        float stepInterp;

        glm::vec3 getDirVecToIdx(std::size_t idx);
        glm::vec3 getPositionFromIdx(std::size_t idx);
        glm::vec3 getScaleFromIdx(std::size_t idx);

        glm::quat lastQuat, rotQuat;

        bool draw;
        bool stop;
};

#endif /* __AIPLAYERFOLLOWERCOMPONENT_HPP__ */
