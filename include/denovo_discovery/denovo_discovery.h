#ifndef PANDORA_DENOVO_DISCOVERY_H
#define PANDORA_DENOVO_DISCOVERY_H

#include "denovo_discovery/candidate_region.h"
#include "denovo_discovery/denovo_utils.h"
#include "denovo_discovery/local_assembly.h"
#include "fastaq_handler.h"
#include <boost/filesystem.hpp>
#include <gatb/debruijn/impl/Simplifications.hpp>
#include <gatb/gatb_core.hpp>
#include <stdexcept>

namespace fs = boost::filesystem;

class DenovoDiscovery {
private:
    const uint16_t kmer_size;
    const double read_error_rate;
    const int max_nb_paths;

public:
    const uint16_t max_insertion_size;
    const uint16_t min_covg_for_node_in_assembly_graph;
    const bool clean_assembly_graph;

    DenovoDiscovery(const uint16_t& kmer_size, const double& read_error_rate,
        int max_nb_paths, uint16_t max_insertion_size = 15,
        uint16_t min_covg_for_node_in_assembly_graph = 1, bool clean = false);

    void find_paths_through_candidate_region(
        CandidateRegion& candidate_region, const fs::path& temp_dir) const;

    double calculate_kmer_coverage(
        const uint32_t& read_covg, const uint32_t& ref_length) const;
};

#endif // PANDORA_DENOVO_DISCOVERY_H
