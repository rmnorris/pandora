#ifndef PANDORA_LOCAL_ASSEMBLY_H
#define PANDORA_LOCAL_ASSEMBLY_H

#include <iostream>
#include <fstream>
#include <stack>
#include <unordered_map>

#include <gatb/gatb_core.hpp>
#include <sys/stat.h>
#include <unistd.h>


const long g_max_length {30};
const int g_local_assembly_kmer_size {9};


using DfsTree = std::unordered_map<std::string, GraphVector<Node>>;
using Paths = std::vector<std::string>;


std::pair<Node, bool> get_node(const std::string &kmer, const Graph &graph);


bool has_ending(std::string const &fullString, std::string const &ending);


DfsTree DFS(const Node &start_node, const Graph &graph);


Paths get_paths_between(const std::string &start_kmer,
                       const std::string &end_kmer,
                       DfsTree &tree,
                       const Graph &graph);

void get_paths_between_util(const std::string &node,
                            const std::string &end_kmer,
                            std::string path_accumulator,
                            const Graph &graph,
                            DfsTree &tree,
                            Paths &full_paths,
                            const long max_length=g_max_length);


void write_paths_to_fasta(const std::string &filepath,
                          Paths &paths,
                          unsigned long line_width = 80);


void local_assembly(const std::string &filepath,
                    std::string &start_kmer,
                    std::string &end_kmer,
                    const std::string &out_path,
                    const int kmer_size=g_local_assembly_kmer_size);

std::string reverse_complement(const std::string forward);

bool file_exists(const std::string& name);

#endif //PANDORA_LOCAL_ASSEMBLY_H
