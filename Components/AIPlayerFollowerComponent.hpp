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
        std::size_t m_currentIdx, m_currTargetIdx;

        glm::vec3 m_dirToCurrTarget;
        bool isMoving, isSameIdx;

        std::size_t findIdx(const glm::vec3 &thisPosition, bool &found);
        glm::vec3 getDirVecToIdx(std::size_t idx);
};

#endif /* __AIPLAYERFOLLOWERCOMPONENT_HPP__ */
