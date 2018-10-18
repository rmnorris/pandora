#ifndef __EXTRACT_READS_H_INCLUDED__   // if extract_reads.h hasn't been included yet...
#define __EXTRACT_READS_H_INCLUDED__

#include <set>
#include <algorithm>
#include <memory>
#include "interval.h"
#include "localnode.h"
#include "minihits.h"
#include "pangenome/ns.cpp"

#include "denovo_discovery/local_assembly.h"
#include "gene_interval_info.h"


using PanNodePtr = std::shared_ptr<pangenome::Node>;

struct ReadCoordinate {
    uint32_t id;
    uint32_t start;
    uint32_t end;
    bool strand;

    bool operator<(const ReadCoordinate &y) const;

    bool operator==(const ReadCoordinate &y) const;
};

std::vector<Interval>
identify_regions(const std::vector<uint32_t> &, const uint32_t &threshold = 0, const uint32_t &min_length = 0);

vector<LocalNodePtr>
find_interval_in_localpath(const Interval &, const vector<LocalNodePtr> &, const unsigned int &buff);

std::set<MinimizerHitPtr, pComp_path> hits_inside_path(const std::set<MinimizerHitPtr, pComp_path> &,
                                                      const std::vector<LocalNodePtr> &);

std::set<ReadCoordinate> get_read_overlap_coordinates(const PanNodePtr &,
                                                      const std::vector<LocalNodePtr> &);

namespace denovo_discovery {
    void add_pnode_coordinate_pairs(std::set<std::pair<ReadCoordinate, GeneIntervalInfo>> &,
                                    const PanNodePtr &,
                                    const std::vector<LocalNodePtr> &,
                                    const std::vector<KmerNodePtr> &,
                                    const uint32_t &padding_size = 0,
                                    const uint32_t &low_coverage_threshold = 2,
                                    const uint32_t &interval_min_length = 5);
}

using ReadPileup = std::vector<std::string>;

std::map<GeneIntervalInfo, ReadPileup>
collect_read_pileups(const std::set<std::pair<ReadCoordinate, GeneIntervalInfo>> &,
                     const std::string &,
                     const uint32_t &padding_size = 0);

void add_pnode_coordinate_pairs(std::vector<std::pair<ReadCoordinate, GeneIntervalInfo>>&,
                                const PanNodePtr ,
                                const vector<LocalNodePtr> &,
                                const vector<KmerNodePtr> &,
                                const unsigned int &buff = 0,
                                const uint32_t &threshold = 2,
                                const uint32_t &min_length = 5);

void save_read_strings_to_denovo_assemble(const std::string &,
                                          const std::string &,
                                          const PanNodePtr,
                                          const std::vector<LocalNodePtr> &,
                                          const std::vector<KmerNodePtr> &,
                                          const unsigned int &buff = 0,
                                          const uint32_t &threshold = 2,
                                          const uint32_t &min_length = 5
);


Interval apply_buffer_to_interval(const Interval &interval, const int32_t &buff);

#endif