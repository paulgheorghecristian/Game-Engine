#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>

class Graph {
    public:
        Graph();

        void addConnection(std::size_t key, std::size_t value);
        void addNode(std::size_t key);
        void printGraph();

        inline std::size_t getNumNodes() { return m_numNodes; }
        inline std::unordered_map<std::size_t, std::unordered_set<std::size_t>> &getNodes() { return m_nodes; }
    private:
        std::unordered_map<std::size_t, std::unordered_set<std::size_t>> m_nodes;
        std::size_t m_numNodes;
};

#endif /* __GRAPH_H__ */