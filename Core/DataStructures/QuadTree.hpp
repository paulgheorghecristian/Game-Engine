#ifndef __QUAD_TREE_HPP__
#define __QUAD_TREE_HPP__

#include <glm/glm.hpp>
#include <vector>

#include "Graph.hpp"

#include <iostream>

class QuadTree {
    public:
    struct CubeBody {
        glm::vec2 xLimits, yLimits;
        glm::vec2 centerPosition;
        int dimension;
        bool isBlocked;
    };

    struct Node {
        Node();

        QuadTree::CubeBody m_data;
        Node *m_children[4];
        int m_level;
        std::size_t m_idx;

        static std::size_t g_IDX;
    };
    public:
        QuadTree(glm::vec2 centerPosition, int dim = 8, int maxLevel = 2);
        ~QuadTree();

        bool divide(QuadTree::Node *node);
        void createGraph(QuadTree::Node *node);
        void searchNeighbours(QuadTree::Node *node,
                                QuadTree::Node *possibleNeighbour,
                                std::vector<QuadTree::Node *> &neighbours);

        inline int getNumLeafs() { return m_numLeafs; }
        inline QuadTree::Node &getRoot() { return m_root; };
        inline Graph &getGraph() { return m_graph; }
        inline int getMaxLevel() { return m_maxLevel; }

    private:
        void clearMem(QuadTree::Node *node);

        Node m_root;
        int m_maxLevel;
        int m_numLeafs;

        Graph m_graph;
};

#endif /* __QUAD_TREE_HPP__ */