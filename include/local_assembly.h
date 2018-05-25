#ifndef PANDORA_LOCAL_ASSEMBLY_H
#define PANDORA_LOCAL_ASSEMBLY_H

#include <iostream>
#include <stack>
#include <unordered_map>
#include <set>

#include <gatb/gatb_core.hpp>

const long g_max_length{1000};

using DfsTree = std::unordered_map<std::string, GraphVector<Node>>;

std::pair<Node, bool> get_node(const std::string &kmer, const Graph &graph);

DfsTree DFS(const Node &start_node, const Graph &graph);

void get_paths_between(const std::string &start_kmer,
                       const std::string &end_kmer,
                       DfsTree &tree,
                       const Graph &graph,
                       std::vector<std::string> &result);

void helper(const std::string &node,
            std::string acc,
            const Graph &graph,
            DfsTree &tree,
            std::vector<std::string> &result);

#endif //PANDORA_LOCAL_ASSEMBLY_H
