#include "QuadTree.hpp"

#include <cmath>

std::size_t QuadTree::Node::g_IDX = 0;

QuadTree::QuadTree(glm::vec2 centerPosition, int dim, int maxLevel): m_maxLevel(maxLevel) {
    m_root.m_data.centerPosition = centerPosition;
    m_root.m_data.dimension = dim;
    m_root.m_data.isBlocked = false;
    m_root.m_data.xLimits = glm::vec2(centerPosition.x - dim / 2, centerPosition.x + dim / 2);
    m_root.m_data.yLimits = glm::vec2(centerPosition.y - dim / 2, centerPosition.y + dim / 2);
    m_numLeafs = 1;

    int dimLog = std::log2(dim);
    if (maxLevel > dimLog) {
        m_maxLevel = dimLog;
    }
}

QuadTree::~QuadTree() {
    clearMem(&m_root);
}

bool QuadTree::divide(QuadTree::Node *node) {
    if (node == NULL || node->m_level > m_maxLevel)
        return false;

    if (node->m_children[0] != NULL ||
        node->m_children[1] != NULL ||
        node->m_children[2] != NULL ||
        node->m_children[3] != NULL)
        return false;

    float halfDim = node->m_data.dimension / 2;
    float quarterDim = halfDim / 2;
    glm::vec2 centerPosition;

    m_numLeafs += 3;

    node->m_children[0] = new QuadTree::Node();
    node->m_children[0]->m_level = node->m_level+1;
    centerPosition = glm::vec2(node->m_data.centerPosition.x - quarterDim,
                                node->m_data.centerPosition.y + quarterDim);
    node->m_children[0]->m_data.centerPosition = centerPosition;
    node->m_children[0]->m_data.dimension = halfDim;
    node->m_children[0]->m_data.xLimits = glm::vec2(centerPosition.x - quarterDim, centerPosition.x + quarterDim);
    node->m_children[0]->m_data.yLimits = glm::vec2(centerPosition.y - quarterDim, centerPosition.y + quarterDim);

    node->m_children[1] = new QuadTree::Node();
    node->m_children[1]->m_level = node->m_level+1;
    centerPosition = glm::vec2(node->m_data.centerPosition.x + quarterDim,
                                node->m_data.centerPosition.y + quarterDim);
    node->m_children[1]->m_data.centerPosition = centerPosition;
    node->m_children[1]->m_data.dimension = halfDim;
    node->m_children[1]->m_data.xLimits = glm::vec2(centerPosition.x - quarterDim, centerPosition.x + quarterDim);
    node->m_children[1]->m_data.yLimits = glm::vec2(centerPosition.y - quarterDim, centerPosition.y + quarterDim);

    node->m_children[2] = new QuadTree::Node();
    node->m_children[2]->m_level = node->m_level+1;
    centerPosition = glm::vec2(node->m_data.centerPosition.x + quarterDim,
                                node->m_data.centerPosition.y - quarterDim);
    node->m_children[2]->m_data.centerPosition = centerPosition;
    node->m_children[2]->m_data.dimension = halfDim;
    node->m_children[2]->m_data.xLimits = glm::vec2(centerPosition.x - quarterDim, centerPosition.x + quarterDim);
    node->m_children[2]->m_data.yLimits = glm::vec2(centerPosition.y - quarterDim, centerPosition.y + quarterDim);

    node->m_children[3] = new QuadTree::Node();
    node->m_children[3]->m_level = node->m_level+1;
    centerPosition = glm::vec2(node->m_data.centerPosition.x - quarterDim,
                                node->m_data.centerPosition.y - quarterDim);
    node->m_children[3]->m_data.centerPosition = centerPosition;
    node->m_children[3]->m_data.dimension = halfDim;
    node->m_children[3]->m_data.xLimits = glm::vec2(centerPosition.x - quarterDim, centerPosition.x + quarterDim);
    node->m_children[3]->m_data.yLimits = glm::vec2(centerPosition.y - quarterDim, centerPosition.y + quarterDim);

    return true;
}

void QuadTree::searchNeighbours(QuadTree::Node *node,
                                    QuadTree::Node *possibleNeighbour,
                                    std::vector<QuadTree::Node *> &neighbours) {
    if (possibleNeighbour == NULL)
        return;

    if (possibleNeighbour->m_children[0] == NULL &&
        possibleNeighbour->m_children[1] == NULL &&
        possibleNeighbour->m_children[2] == NULL &&
        possibleNeighbour->m_children[3] == NULL &&
        possibleNeighbour->m_data.isBlocked == false) {

        float thisNodeXMin = node->m_data.xLimits.x;
        float thisNodeXMax = node->m_data.xLimits.y;

        float thisNodeYMin = node->m_data.yLimits.x;
        float thisNodeYMax = node->m_data.yLimits.y;

        float neighXMin = possibleNeighbour->m_data.xLimits.x;
        float neighXMax = possibleNeighbour->m_data.xLimits.y;

        float neighYMin = possibleNeighbour->m_data.yLimits.x;
        float neighYMax = possibleNeighbour->m_data.yLimits.y;

        // check boundaries
        if (thisNodeYMin <= neighYMin &&
            thisNodeYMax >= neighYMax &&
            (thisNodeXMin == neighXMax ||
            thisNodeXMax == neighXMin)) {
            // left and right
            neighbours.push_back(possibleNeighbour);
        } else if (thisNodeXMin <= neighXMin &&
                    thisNodeXMax >= neighXMax &&
                    (thisNodeYMin == neighYMax ||
                    thisNodeYMax == neighYMin)) {
            // up and down
            neighbours.push_back(possibleNeighbour);
        } else if (thisNodeYMax == neighYMin &&
                    thisNodeXMin == neighXMax) {
            // LU
            neighbours.push_back(possibleNeighbour);
        } else if (thisNodeYMax == neighYMin &&
                    thisNodeXMax == neighXMin) {
            // RU
            neighbours.push_back(possibleNeighbour);
        } else if (thisNodeYMin == neighYMax &&
                    thisNodeXMin == neighXMax) {
            // LD
            neighbours.push_back(possibleNeighbour);
        } else if (thisNodeYMin == neighYMax &&
                    thisNodeXMax == neighXMin) {
            // RD
            neighbours.push_back(possibleNeighbour);
        }
    } else {
        searchNeighbours(node, possibleNeighbour->m_children[0], neighbours);
        searchNeighbours(node, possibleNeighbour->m_children[1], neighbours);
        searchNeighbours(node, possibleNeighbour->m_children[2], neighbours);
        searchNeighbours(node, possibleNeighbour->m_children[3], neighbours);
    }
}

void QuadTree::createGraph(QuadTree::Node *node) {
    if (node == NULL)
        return;

    m_graph.addNode(node->m_idx);

    if (node->m_children[0] == NULL &&
        node->m_children[1] == NULL &&
        node->m_children[2] == NULL &&
        node->m_children[3] == NULL &&
        node->m_data.isBlocked == false) {
        //leaf
        std::vector<QuadTree::Node *> neighbours(0);

        // float thisNodeXMin = node->m_data.xLimits.x;
        // float thisNodeXMax = node->m_data.xLimits.y;

        // float thisNodeYMin = node->m_data.yLimits.x;
        // float thisNodeYMax = node->m_data.yLimits.y;

        searchNeighbours(node, &m_root, neighbours);
        // std::cout << "node: " << node->m_idx << std::endl;
        // std::cout << "(xmin,xmax) " << thisNodeXMin << " " << thisNodeXMax << std::endl;
        // std::cout << "(ymin,ymax) " << thisNodeYMin << " " << thisNodeYMax << std::endl;        
        // std::cout << "neigh:" << std::endl;
        // for (QuadTree::Node *n : neighbours) {
        //     std::cout << n->m_idx << " ";
        // }
        // std::cout << std::endl;
        if (neighbours.size() > 0) {
            for (QuadTree::Node *n: neighbours) {
                m_graph.addConnection(node->m_idx, n->m_idx);
                m_graph.addConnection(n->m_idx, node->m_idx);
            }
        }
    } else {
        createGraph(node->m_children[0]);
        createGraph(node->m_children[1]);
        createGraph(node->m_children[2]);
        createGraph(node->m_children[3]);
    }
}

void QuadTree::clearMem(QuadTree::Node *node) {
    if (node == NULL)
        return;

    clearMem(node->m_children[0]);
    delete node->m_children[0];
    node->m_children[0] = NULL;

    clearMem(node->m_children[1]);
    delete node->m_children[1];
    node->m_children[1] = NULL;

    clearMem(node->m_children[2]);
    delete node->m_children[2];
    node->m_children[2] = NULL;

    clearMem(node->m_children[3]);
    delete node->m_children[3];
    node->m_children[3] = NULL;
}

QuadTree::Node::Node() {
    m_children[0] = NULL;
    m_children[1] = NULL;
    m_children[2] = NULL;
    m_children[3] = NULL;
    m_level = 0;
    m_idx = QuadTree::Node::g_IDX++;
}
