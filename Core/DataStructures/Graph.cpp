#include "Graph.hpp"

Graph::Graph() {
    m_numNodes = 0;
}

void Graph::addConnection(std::size_t key, std::size_t value) {
    if (key > m_numNodes)
        m_numNodes = key;

    std::unordered_set<std::size_t> &values = m_nodes[key];

    values.insert(value);
}

void Graph::addNode(std::size_t key) {
    (void) m_nodes[key];
}

void Graph::printGraph() {
    std::cout << "num_nodes=" << m_numNodes << std::endl;
    for (std::size_t n = 1; n <= m_numNodes; n++) {
        std::unordered_set<std::size_t> &values = m_nodes[n];

        if (values.size() > 0) {
            std::cout << "n=" << n << std::endl;
            for (std::size_t v: values) {
                std::cout << v << " ";
            }
            std::cout << std::endl;
        }
    }
}