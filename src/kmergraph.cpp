#include <sstream>
#include <fstream>
#include <cassert>
#include <limits>
#include <cstdio>      /* NULL */
#include <cstdlib>     /* srand, rand */
#include <cmath>

#include <boost/math/distributions/negative_binomial.hpp>
#include <boost/log/trivial.hpp>

#include "utils.h"
#include "kmernode.h"
#include "kmergraph.h"
#include "localPRG.h"


#define assert_msg(x) !(std::cerr << "Assertion failed: " << x << std::endl)


using namespace prg;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//KmerGraph methods definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
KmerGraph::KmerGraph() {
    shortest_path_length = 0;
    k = 0; // nb the kmer size is determined by the first non-null node added
}

// copy constructor
KmerGraph::KmerGraph(const KmerGraph &other) {
    shortest_path_length = other.shortest_path_length;
    k = other.k;
    KmerNodePtr n;

    // first we need to deallocate for any nodes already got!
    clear();
    nodes.reserve(other.nodes.size());

    // create deep copies of the nodes, minus the edges
    for (const auto &node : other.nodes) {
        n = std::make_shared<KmerNode>(*node);
        assert(nodes.size() == n->id);
        nodes.push_back(n);
        sorted_nodes.insert(n);
    }

    // now need to copy the edges
    for (const auto &node : other.nodes) {
        for (uint32_t j = 0; j < node->out_nodes.size(); ++j) {
            add_edge(nodes.at(node->id), nodes.at(node->out_nodes[j].lock()->id));
        }
    }
}

// Assignment operator
KmerGraph &KmerGraph::operator=(const KmerGraph &other) {
    // check for self-assignment
    if (this == &other)
        return *this;

    // first we need to deallocate for any nodes already got!
    clear();
    nodes.reserve(other.nodes.size());

    // shallow copy no pointers
    shortest_path_length = other.shortest_path_length;
    k = other.k;
    KmerNodePtr n;

    // create deep copies of the nodes, minus the edges
    for (const auto &node : other.nodes) {
        n = std::make_shared<KmerNode>(*node);
        assert(nodes.size() == n->id);
        nodes.push_back(n);
        sorted_nodes.insert(n);
    }

    // now need to copy the edges
    for (const auto &node : other.nodes) {
        for (uint32_t j = 0; j < node->out_nodes.size(); ++j) {
            add_edge(nodes.at(node->id), nodes.at(node->out_nodes[j].lock()->id));
        }
    }

    return *this;
}

void KmerGraph::clear() {
    nodes.clear();
    assert(nodes.empty());

    sorted_nodes.clear();
    assert(sorted_nodes.empty());

    shortest_path_length = 0;
    k = 0;
}

KmerNodePtr KmerGraph::add_node(const prg::Path &p) { //add this kmer path to this kmer graph
    for (const auto &c : nodes) { //check if this kmer path is already added
        if (c->path == p) { //TODO: overload operator == to receive a prg::Path?
            return c;
        }
    }

    // if we didn't find an existing node, add this kmer path to the graph
    KmerNodePtr n(std::make_shared<KmerNode>(nodes.size(), p)); //create the node
    nodes.push_back(n); //add it to nodes
    sorted_nodes.insert(n);
    //nodes[next_id] = make_shared<KmerNode>(next_id, p);
    assert(k == 0 or p.length() == 0 or p.length() == k);
    if (k == 0 and p.length() > 0) {
        k = p.length();
    }
    assert(nodes.size() < std::numeric_limits<uint32_t>::max() ||
           assert_msg("WARNING, reached max kmer graph node size"));
    if (nodes.size() == reserved_size) {
        reserved_size *= 2;
        nodes.reserve(reserved_size);
    }
    return n;
}

KmerNodePtr KmerGraph::add_node_with_kh(const prg::Path &p, const uint64_t &kh, const uint8_t &num) {
    KmerNodePtr n = add_node(p);
    n->khash = kh;
    n->num_AT = num;
    assert(n->khash < std::numeric_limits<uint64_t>::max());
    return n;
}

condition::condition(const prg::Path &p) : q(p) {};

bool condition::operator()(const KmerNodePtr kn) const { return kn->path == q; }

void KmerGraph::add_edge(KmerNodePtr from, KmerNodePtr to) {
    assert(from->id < nodes.size() and nodes[from->id] == from);
    assert(to->id < nodes.size() and nodes[to->id] == to);
    assert(from->path < to->path
           or assert_msg(
            "Cannot add edge from " << from->id << " to " << to->id << " because " << from->path << " is not less than "
                                    << to->path));

    if (from->find_node_ptr_in_out_nodes(to) == from->out_nodes.end()) {
        from->out_nodes.emplace_back(to);
        to->in_nodes.emplace_back(from);
        //cout << "added edge from " << from->id << " to " << to->id << endl;
    }
}

void KmerGraph::remove_shortcut_edges() {
    BOOST_LOG_TRIVIAL(debug) << "Remove 'bad' edges from kmergraph";
    prg::Path temp_path;
    uint32_t num_removed_edges = 0;
    std::vector<KmerNodePtr> v = {};
    std::deque<std::vector<KmerNodePtr>> d;

    for (const auto &n : nodes) {
        //cout << n.first << endl;
        for (const auto &out : n->out_nodes) {
            auto out_node_as_shared_ptr = out.lock();
            for (auto nextOut = out_node_as_shared_ptr->out_nodes.begin(); nextOut != out_node_as_shared_ptr->out_nodes.end();) {
                auto nextOutAsSharedPtr = nextOut->lock();
                // if the outnode of an outnode of A is another outnode of A
                if (n->find_node_ptr_in_out_nodes(nextOutAsSharedPtr) != n->out_nodes.end()) {
                    temp_path = get_union(n->path, nextOutAsSharedPtr->path);

                    if (out_node_as_shared_ptr->path.is_subpath(temp_path)) {
                        //remove it from the outnodes
                        BOOST_LOG_TRIVIAL(debug) << "found the union of " << n->path << " and " << nextOutAsSharedPtr->path;
                        BOOST_LOG_TRIVIAL(debug) << "result " << temp_path << " contains " << out_node_as_shared_ptr->path;
                        nextOutAsSharedPtr->in_nodes.erase(
                                nextOutAsSharedPtr->find_node_ptr_in_in_nodes(out_node_as_shared_ptr));
                        nextOut = out_node_as_shared_ptr->out_nodes.erase(nextOut);
                        BOOST_LOG_TRIVIAL(debug) << "next out is now " << nextOutAsSharedPtr->path;
                        num_removed_edges += 1;
                        break;
                    } else {
                        nextOut++;
                    }
                } else {
                    nextOut++;
                }
            }
        }
    }
    BOOST_LOG_TRIVIAL(debug) << "Found and removed " << num_removed_edges << " edges from the kmergraph";
}

void KmerGraph::check() const {
    // should not have any leaves, only nodes with degree 0 are start and end
    for (auto c = sorted_nodes.begin(); c != sorted_nodes.end(); ++c) {
        assert(!(*c)->in_nodes.empty() or (*c) == (*sorted_nodes.begin())||
               assert_msg("node" << **c << " has inNodes size " << (*c)->in_nodes.size()));
        assert(!(*c)->out_nodes.empty() or (*c) == *(sorted_nodes.rbegin()) || assert_msg(
                "node" << **c << " has outNodes size " << (*c)->out_nodes.size() << " and isn't equal to back node "
                       << **(sorted_nodes.rbegin())));
        for (const auto &d: (*c)->out_nodes) {
            auto dAsSharedPtr = d.lock();
            assert((*c)->path < dAsSharedPtr->path || assert_msg((*c)->path << " is not less than " << dAsSharedPtr->path));
            assert(find(c, sorted_nodes.end(), dAsSharedPtr) != sorted_nodes.end() ||
                   assert_msg(dAsSharedPtr->id << " does not occur later in sorted list than " << (*c)->id));
        }
    }
}

void KmerGraph::discover_k() {
    if (nodes.size() > 0) {
        auto it = nodes.begin();
        it++;
        const auto &knode = **it;
        k = knode.path.length();
    }
}

std::ostream &operator<<(std::ostream &out, KmerGraph const &data) {
    for (const auto &c: data.nodes) {
        out << *(c) << std::endl;
    }
    return out;
}

//save the KmerGraph as gfa
void KmerGraph::save(const std::string &filepath, const std::shared_ptr<LocalPRG> localprg) {
    uint32_t sample_id = 0;

    std::ofstream handle;
    handle.open(filepath);
    if (handle.is_open()) {
        handle << "H\tVN:Z:1.0\tbn:Z:--linear --singlearr" << std::endl;
        for (const auto &c : nodes) {
            handle << "S\t" << c->id << "\t";

            if (localprg != nullptr) {
                handle << localprg->string_along_path(c->path);
            } else {
                handle << c->path;
            }

            //TODO: leave as coverage 0 or change this?
            handle << "\tFC:i:" << 0 << "\t" << "\tRC:i:" << 0 << std::endl;//"\t" << (unsigned)nodes[i].second->num_AT << endl;

            for (uint32_t j = 0; j < c->out_nodes.size(); ++j) {
                handle << "L\t" << c->id << "\t+\t" << c->out_nodes[j].lock()->id << "\t+\t0M" << std::endl;
            }
        }
        handle.close();
    } else {
        std::cerr << "Unable to open kmergraph file " << filepath << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void KmerGraph::load(const std::string &filepath) {
    clear();
    uint32_t sample_id = 0;

    std::string line;
    std::vector<std::string> split_line;
    std::stringstream ss;
    uint32_t id = 0, covg, from, to;
    prg::Path p;
    uint32_t num_nodes = 0;

    std::ifstream myfile(filepath);
    if (myfile.is_open()) {

        while (getline(myfile, line).good()) {
            if (line[0] == 'S') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 4);
                id = std::stoi(split_line[1]);
                num_nodes = std::max(num_nodes, id);
            }
        }
        myfile.clear();
        myfile.seekg(0, myfile.beg);
        nodes.reserve(num_nodes);
        std::vector<uint16_t> outnode_counts(num_nodes + 1, 0), innode_counts(num_nodes + 1, 0);

        while (getline(myfile, line).good()) {
            if (line[0] == 'S') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 4);
                id = stoi(split_line[1]);
                ss << split_line[2];
                char c = ss.peek();
                assert (isdigit(c) or assert_msg("Cannot read in this sort of kmergraph GFA as it does not label nodes "
                                                 "with their PRG path"));
                ss >> p;
                ss.clear();
                //add_node(p);
                KmerNodePtr kmer_node = std::make_shared<KmerNode>(id, p);
                assert(kmer_node != nullptr);
                assert(id == nodes.size() or num_nodes - id == nodes.size() or
                       assert_msg("id " << id << " != " << nodes.size() << " nodes.size() for kmergraph "));
                nodes.push_back(kmer_node);
                sorted_nodes.insert(kmer_node);
                if (k == 0 and p.length() > 0) {
                    k = p.length();
                }
                covg = stoi(split(split_line[3], "FC:i:")[0]);
                //kmer_node->set_covg(covg, 0, sample_id); //TODO: do not read the coverage?
                covg = stoi(split(split_line[4], "RC:i:")[0]);
                //kmer_node->set_covg(covg, 1, sample_id); //TODO: do not read the coverage?
                if (split_line.size() >= 6) {
                    kmer_node->num_AT = std::stoi(split_line[5]);
                }
            } else if (line[0] == 'L') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 5);
                assert(stoi(split_line[1]) < (int) outnode_counts.size() or
                       assert_msg(stoi(split_line[1]) << ">=" << outnode_counts.size()));
                assert(stoi(split_line[3]) < (int) innode_counts.size() or
                       assert_msg(stoi(split_line[3]) << ">=" << innode_counts.size()));
                outnode_counts[stoi(split_line[1])] += 1;
                innode_counts[stoi(split_line[3])] += 1;
            }
        }

        if (id == 0) {
            reverse(nodes.begin(), nodes.end());
        }

        id = 0;
        for (const auto &n : nodes) {
            assert(nodes[id]->id == id);
            id++;
            assert(n->id < outnode_counts.size() or assert_msg(n->id << ">=" << outnode_counts.size()));
            assert(n->id < innode_counts.size() or assert_msg(n->id << ">=" << innode_counts.size()));
            n->out_nodes.reserve(outnode_counts[n->id]);
            n->in_nodes.reserve(innode_counts[n->id]);
        }

        myfile.clear();
        myfile.seekg(0, myfile.beg);

        while (getline(myfile, line).good()) {
            if (line[0] == 'L') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 5);
                if (split_line[2] == split_line[4]) {
                    from = std::stoi(split_line[1]);
                    to = std::stoi(split_line[3]);
                } else {
                    // never happens
                    from = std::stoi(split_line[3]);
                    to = std::stoi(split_line[1]);
                }
                add_edge(nodes[from], nodes[to]);
                //nodes[from]->outNodes.push_back(nodes.at(to));
                //nodes[to]->inNodes.push_back(nodes.at(from));
            }
        }
    } else {
        std::cerr << "Unable to open kmergraph file " << filepath << std::endl;
        exit(1);
    }
}


uint32_t KmerGraph::min_path_length() {
    //TODO: FIX THIS INNEFICIENCY I INTRODUCED
    std::vector<KmerNodePtr> sorted_nodes(this->sorted_nodes.begin(), this->sorted_nodes.end());

    if (shortest_path_length > 0) {
        return shortest_path_length;
    }

#ifndef NDEBUG
    //TODO: check if tests must be updated or not due to this (I think not - sorted_nodes is always sorted)
    //if this is added, some tests bug, since it was not executed before...
    //check();
#endif

    std::vector<uint32_t> len(sorted_nodes.size(), 0); // length of shortest path from node i to end of graph
    for (uint32_t j = sorted_nodes.size() - 1; j != 0; --j) {
        for (uint32_t i = 0; i != sorted_nodes[j - 1]->out_nodes.size(); ++i) {
            if (len[sorted_nodes[j - 1]->out_nodes[i].lock()->id] + 1 > len[j - 1]) {
                len[j - 1] = len[sorted_nodes[j - 1]->out_nodes[i].lock()->id] + 1;
            }
        }
    }
    shortest_path_length = len[0];
    return len[0];
}

bool KmerGraph::operator==(const KmerGraph &y) const {
    // false if have different numbers of nodes
    if (y.nodes.size() != nodes.size()) {//cout << "different numbers of nodes" << endl;
        return false;
    }

    // false if have different nodes
    for (const auto &kmer_node_ptr: nodes) {
        const auto &kmer_node = *kmer_node_ptr;
        // if node not equal to a node in y, then false
        auto found = find_if(y.nodes.begin(), y.nodes.end(), condition(kmer_node.path));
        if (found == y.nodes.end()) {
            return false;
        }

        // if the node is found but has different edges, then false
        if (kmer_node.out_nodes.size() != (*found)->out_nodes.size()) { return false; }
        if (kmer_node.in_nodes.size() != (*found)->in_nodes.size()) { return false; }
        for (uint32_t j = 0; j != kmer_node.out_nodes.size(); ++j) {
            if ((*found)->find_node_in_out_nodes(*(kmer_node.out_nodes[j].lock())) == (*found)->out_nodes.end()) {
                return false;
            }
        }

    }
    // otherwise is true
    return true;
}

bool pCompKmerNode::operator()(KmerNodePtr lhs, KmerNodePtr rhs) {
    return (lhs->path) < (rhs->path);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//KmerGraph methods definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//KmerGraphWithCoverage methods definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void KmerGraphWithCoverage::set_exp_depth_covg(const uint32_t edp) {
    assert(edp > 0);
    exp_depth_covg = edp;
}

void KmerGraphWithCoverage::set_p(const float e_rate) {
    BOOST_LOG_TRIVIAL(debug) << "Set p in kmergraph";
    assert(kmer_prg->k != 0);
    assert(0 < e_rate and e_rate < 1);
    p = 1 / exp(e_rate * kmer_prg->k);
    //cout << "using p: " << p << endl;
}

void KmerGraphWithCoverage::increment_covg(uint32_t node_id, bool strand, uint32_t sample_id) {
    assert(this->nodeIndex2SampleCoverage[node_id].size() > sample_id);

    if (strand)
        this->nodeIndex2SampleCoverage[node_id][sample_id].first++;
    else
        this->nodeIndex2SampleCoverage[node_id][sample_id].second++;
}

uint32_t KmerGraphWithCoverage::get_covg(uint32_t node_id, bool strand, uint32_t sample_id) const {
    if (this->nodeIndex2SampleCoverage[node_id].size() <= sample_id)
        return 0;

    if (strand)
        return this->nodeIndex2SampleCoverage[node_id][sample_id].first;
    else
        return this->nodeIndex2SampleCoverage[node_id][sample_id].second;
}

void KmerGraphWithCoverage::set_covg(uint32_t node_id, uint32_t value, bool strand, uint32_t sample_id) {
    assert(this->nodeIndex2SampleCoverage[node_id].size() > sample_id);
    if (strand)
        this->nodeIndex2SampleCoverage[node_id][sample_id].first = value;
    else
        this->nodeIndex2SampleCoverage[node_id][sample_id].second = value;
}



void KmerGraphWithCoverage::set_nb(const float &nb_prob, const float &nb_fail) {
    if (nb_prob == 0 and nb_fail == 0)
        return;
    //qcout << "set nb" << endl;
    assert((nb_p > 0 and nb_p < 1) || assert_msg("nb_p " << nb_p << " was not set in kmergraph"));
    assert(nb_r > 0 || assert_msg("nb_r was not set in kmergraph"));
    nb_p += nb_prob;
    nb_r += nb_fail;
}


float KmerGraphWithCoverage::nb_prob(uint32_t j, const uint32_t &sample_id) {
    auto k = get_covg(j, 0, sample_id) + get_covg(j, 1, sample_id);
    //cout << "j: " << j << " " << nodes[j]->covg[0] << " + " << nodes[j]->covg[1] << " = "
    //     << nodes[j]->covg[0] + nodes[j]->covg[1] << " = " << k << endl;
    //cout << "nb_prob(" << nb_r << "," << nb_p << "," << k << ") = ";
    float ret = log(pdf(boost::math::negative_binomial(nb_r, nb_p), k));
    ret = std::max(ret, std::numeric_limits<float>::lowest() / 1000);
    //cout << ret << endl;
    return ret;
}

float KmerGraphWithCoverage::lin_prob(uint32_t j, const uint32_t &sample_id) {
    assert(num_reads != 0);
    auto k = get_covg(j, 0, sample_id) + get_covg(j, 1, sample_id);
    return log(float(k)/num_reads);
}

float KmerGraphWithCoverage::prob(uint32_t j, const uint32_t &sample_id) {
    assert(num_reads != 0);
    return prob(j, num_reads, sample_id);
}

float KmerGraphWithCoverage::prob(const uint32_t &j, const uint32_t &num, const uint32_t &sample_id) {
    //prob of node j where j is node id (hence pos in nodes)
    assert(p != 1);
    assert(j < kmer_prg->nodes.size());
    #ifndef NDEBUG
        //TODO: check if tests must be updated or not due to this (I think not - sorted_nodes is always sorted)
        //if this is added, some tests bug, since it was not executed before...
        //check();
    #endif

    uint32_t sum_coverages = get_covg(j, 0, sample_id) + get_covg(j, 1, sample_id);

    float ret;
    if (j == (*(kmer_prg->sorted_nodes.begin()))->id or j == (*(kmer_prg->sorted_nodes.rbegin()))->id) {
        ret = 0; // is really undefined
    } else if (sum_coverages > num) {
        // under model assumptions this can't happen, but it inevitably will, so bodge
        ret = lognchoosek2(sum_coverages,
                           get_covg(j, 0, sample_id),
                           get_covg(j, 1, sample_id))
              + sum_coverages * log(p / 2);
        // note this may give disadvantage to repeat kmers
    } else {
        ret = lognchoosek2(num,
                           get_covg(j, 0, sample_id),
                           get_covg(j, 1, sample_id))
              + sum_coverages * log(p / 2)
              + (num - sum_coverages) * log(1 - p);
    }
    //cout << " is " << ret << endl;
    return ret;
}

bool KmerGraphWithCoverage::coverage_is_zeroes(const uint32_t& sample_id){
    bool all_zero = true;
    for (const auto &n : kmer_prg->nodes) {
        if (get_covg(n->id, 0, sample_id) + get_covg(n->id, 1, sample_id) > 0) {
            BOOST_LOG_TRIVIAL(debug) << "Found non-zero coverage in kmer graph";
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        BOOST_LOG_TRIVIAL(debug) << "ALL ZEROES in kmer graph coverages";
    }
    return all_zero;
}

float KmerGraphWithCoverage::find_max_path(std::vector<KmerNodePtr> &maxpath, const uint32_t &sample_id) {
    //TODO: UPDATE THIS WHEN LEARNING MAP
    //TODO: THESE 3 FUNCTIONS MUST BE REFACTORED: find_max_path(), find_nb_max_path() and find_lin_max_path()
    //TODO: FIX THIS INNEFICIENCY I INTRODUCED
    std::vector<KmerNodePtr> sorted_nodes(this->kmer_prg->sorted_nodes.begin(), this->kmer_prg->sorted_nodes.end());


    // finds a max likelihood path
    BOOST_LOG_TRIVIAL(debug) << "Find max path for sample_id" << sample_id;

    // check if p set
    assert(p < 1 || assert_msg("p was not set in kmergraph"));
    assert(num_reads > 0 || assert_msg("num_reads was not set in kmergraph"));

    // need to catch if thesh not set too...
    kmer_prg->check();

    // also check not all 0 covgs
    auto coverages_all_zero = coverage_is_zeroes(sample_id);
    if (coverages_all_zero)
        return std::numeric_limits<float>::lowest();

    // create vectors to hold the intermediate values
    std::vector<float> M(kmer_prg->nodes.size(), 0); // max log prob pf paths from pos i to end of graph
    std::vector<int> len(kmer_prg->nodes.size(), 0); // length of max log path from pos i to end of graph
    std::vector<uint32_t> prev(kmer_prg->nodes.size(), kmer_prg->nodes.size() - 1); // prev node along path
    float max_mean;
    int max_len;

    for (uint32_t j = kmer_prg->nodes.size() - 1; j != 0; --j) {
        max_mean = std::numeric_limits<float>::lowest();
        max_len = 0; // tie break with longest kmer path
        for (uint32_t i = 0; i != sorted_nodes[j - 1]->out_nodes.size(); ++i) {
            auto node_id = sorted_nodes[j - 1]->out_nodes[i].lock()->id;
            if ((node_id == sorted_nodes.back()->id and thresh > max_mean + 0.000001) or
                (M[node_id] / len[node_id] >
                 max_mean + 0.000001) or
                (max_mean - M[node_id] / len[node_id] <=
                 0.000001 and
                 len[node_id] > max_len)) {
                M[sorted_nodes[j - 1]->id] =
                        prob(sorted_nodes[j - 1]->id, sample_id) + M[node_id];
                len[sorted_nodes[j - 1]->id] = 1 + len[node_id];
                prev[sorted_nodes[j - 1]->id] = node_id;
                if (node_id != sorted_nodes.back()->id) {
                    max_mean = M[node_id] / len[node_id];
                    max_len = len[node_id];
                    //cout << " and new max_mean: " << max_mean;
                } else {
                    max_mean = thresh;
                }
            }
        }
    }
    // remove the final length added for the null start node
    len[0] -= 1;

    // extract path
    uint32_t prev_node = prev[sorted_nodes[0]->id];
    while (prev_node < sorted_nodes.size() - 1) {
        maxpath.push_back(kmer_prg->nodes[prev_node]);
        prev_node = prev[prev_node];
    }

    assert(len[0] > 0 || assert_msg("found no path through kmer prg"));
    return M[0] / len[0];
}

float KmerGraphWithCoverage::find_nb_max_path(std::vector<KmerNodePtr> &maxpath, const uint32_t &sample_id) {
    //TODO: UPDATE THIS WHEN LEARNING MAP
    //TODO: THESE 3 FUNCTIONS MUST BE REFACTORED: find_max_path(), find_nb_max_path() and find_lin_max_path()
    //TODO: FIX THIS INNEFICIENCY I INTRODUCED
    std::vector<KmerNodePtr> sorted_nodes(this->kmer_prg->sorted_nodes.begin(), this->kmer_prg->sorted_nodes.end());


    // finds a max likelihood path
    kmer_prg->check();

    // also check not all 0 covgs
    auto coverages_all_zero = coverage_is_zeroes(sample_id);
    if (coverages_all_zero)
        return std::numeric_limits<float>::lowest();

    // create vectors to hold the intermediate values
    std::vector<float> M(kmer_prg->nodes.size(), 0); // max log prob pf paths from pos i to end of graph
    std::vector<int> len(kmer_prg->nodes.size(), 0); // length of max log path from pos i to end of graph
    std::vector<uint32_t> prev(kmer_prg->nodes.size(), kmer_prg->nodes.size() - 1); // prev node along path
    float max_mean;
    int max_len;

    for (uint32_t j = kmer_prg->nodes.size() - 1; j != 0; --j) {
        max_mean = std::numeric_limits<float>::lowest();
        max_len = 0; // tie break with longest kmer path
        for (uint32_t i = 0; i != sorted_nodes[j - 1]->out_nodes.size(); ++i) {
            auto node_id = sorted_nodes[j - 1]->out_nodes[i].lock()->id;
            if ((node_id == sorted_nodes.back()->id and thresh > max_mean + 0.000001) or
                (M[node_id] / len[node_id] >
                 max_mean + 0.000001) or
                (max_mean - M[node_id] / len[node_id] <=
                 0.000001 and
                 len[node_id] > max_len)) {
                M[sorted_nodes[j - 1]->id] =
                        nb_prob(sorted_nodes[j - 1]->id, sample_id) + M[node_id];
                len[sorted_nodes[j - 1]->id] = 1 + len[node_id];
                prev[sorted_nodes[j - 1]->id] = node_id;
                if (node_id != sorted_nodes.back()->id) {
                    max_mean = M[node_id] / len[node_id];
                    max_len = len[node_id];
                } else {
                    max_mean = thresh;
                }
            }
        }
    }
    // remove the final length added for the null start node
    len[0] -= 1;

    // extract path
    uint32_t prev_node = prev[sorted_nodes[0]->id];
    while (prev_node < sorted_nodes.size() - 1) {
        maxpath.push_back(kmer_prg->nodes[prev_node]);
        prev_node = prev[prev_node];
    }

    assert(len[0] > 0 || assert_msg("found no path through kmer prg"));
    return M[0] / len[0];
}

float KmerGraphWithCoverage::find_lin_max_path(std::vector<KmerNodePtr> &maxpath, const uint32_t &sample_id) {
    //TODO: UPDATE THIS WHEN LEARNING MAP
    //TODO: THESE 3 FUNCTIONS MUST BE REFACTORED: find_max_path(), find_nb_max_path() and find_lin_max_path()
    //TODO: FIX THIS INNEFICIENCY I INTRODUCED
    std::vector<KmerNodePtr> sorted_nodes(this->kmer_prg->sorted_nodes.begin(), this->kmer_prg->sorted_nodes.end());


    // finds a max likelihood path
    kmer_prg->check();

    // also check not all 0 covgs
    auto coverages_all_zero = coverage_is_zeroes(sample_id);
    if (coverages_all_zero)
        return std::numeric_limits<float>::lowest();

    // create vectors to hold the intermediate values
    std::vector<float> M(kmer_prg->nodes.size(), 0); // max log prob pf paths from pos i to end of graph
    std::vector<int> len(kmer_prg->nodes.size(), 0); // length of max log path from pos i to end of graph
    std::vector<uint32_t> prev(kmer_prg->nodes.size(), kmer_prg->nodes.size() - 1); // prev node along path
    float max_mean;
    int max_len;

    for (uint32_t j = kmer_prg->nodes.size() - 1; j != 0; --j) {
        max_mean = std::numeric_limits<float>::lowest();
        max_len = 0; // tie break with longest kmer path
        for (uint32_t i = 0; i != sorted_nodes[j - 1]->out_nodes.size(); ++i) {
            auto node_id = sorted_nodes[j - 1]->out_nodes[i].lock()->id;
            if ((node_id == sorted_nodes.back()->id and thresh > max_mean + 0.000001) or
                (M[node_id] / len[node_id] >
                 max_mean + 0.000001) or
                (max_mean - M[node_id] / len[node_id] <=
                 0.000001 and
                 len[node_id] > max_len)) {
                M[sorted_nodes[j - 1]->id] =
                        lin_prob(sorted_nodes[j - 1]->id, sample_id) + M[node_id];
                len[sorted_nodes[j - 1]->id] = 1 + len[node_id];
                prev[sorted_nodes[j - 1]->id] = node_id;
                if (node_id != sorted_nodes.back()->id) {
                    max_mean = M[node_id] / len[node_id];
                    max_len = len[node_id];
                } else {
                    max_mean = thresh;
                }
            }
        }
    }
    // remove the final length added for the null start node
    len[0] -= 1;

    // extract path
    uint32_t prev_node = prev[sorted_nodes[0]->id];
    while (prev_node < kmer_prg->sorted_nodes.size() - 1) {
        maxpath.push_back(kmer_prg->nodes[prev_node]);
        prev_node = prev[prev_node];
        if (maxpath.size() > 1000000){
            BOOST_LOG_TRIVIAL(warning) << "I think I've found an infinite loop - is something wrong with this kmergraph?";
            exit(1);
        }
    }

    assert(len[0] > 0 || assert_msg("found no path through kmer prg"));
    return M[0] / len[0];
}

/*
std::vector<std::vector<KmerNodePtr>> KmerGraph::find_max_paths(uint32_t num,
                                                                const uint32_t &sample_id) {

    // save original coverges so can put back at the end
    std::vector<uint32_t> original_covgs0, original_covgs1;
    for (uint32_t i = 0; i != nodes.size(); ++i) {
        original_covgs0.push_back(nodes[i]->get_covg(0, sample_id));
        original_covgs1.push_back(nodes[i]->get_covg(1, sample_id));
    }

    // find num max paths
    //cout << "expected covg " << (uint)(p*num_reads/num) << endl;
    std::vector<std::vector<KmerNodePtr>> paths;
    std::vector<KmerNodePtr> maxpath;
    find_max_path(maxpath, <#initializer#>);
    //uint min_covg;
    paths.push_back(maxpath);

    while (paths.size() < num) {
        for (uint32_t i = 0; i != maxpath.size(); ++i) {
            maxpath[i]->covg[0] -= std::min(maxpath[i]->covg[0], (uint32_t) (p * num_reads / num));
            maxpath[i]->covg[1] -= std::min(maxpath[i]->covg[1], (uint32_t) (p * num_reads / num));
        }
        maxpath.clear();
        find_max_path(maxpath, <#initializer#>);
        paths.push_back(maxpath);
    }

    // put covgs back
    for (uint32_t i = 0; i != nodes.size(); ++i) {
        nodes[i]->covg[0] = original_covgs0[i];
        nodes[i]->covg[1] = original_covgs1[i];
    }

    return paths;
}
 */

std::vector<std::vector<KmerNodePtr>> KmerGraphWithCoverage::get_random_paths(uint32_t num_paths) {
    // find a random path through kmergraph picking ~uniformly from the outnodes at each point
    std::vector<std::vector<KmerNodePtr>> rpaths;
    std::vector<KmerNodePtr> rpath;
    uint32_t i;

    time_t now;
    now = time(nullptr);
    srand((unsigned int) now);

    if (!kmer_prg->nodes.empty()) {
        for (uint32_t j = 0; j != num_paths; ++j) {
            i = rand() % kmer_prg->nodes[0]->out_nodes.size();
            rpath.push_back(kmer_prg->nodes[0]->out_nodes[i].lock());
            while (rpath.back() != kmer_prg->nodes[kmer_prg->nodes.size() - 1]) {
                if (rpath.back()->out_nodes.size() == 1) {
                    rpath.push_back(rpath.back()->out_nodes[0].lock());
                } else {
                    i = rand() % rpath.back()->out_nodes.size();
                    rpath.push_back(rpath.back()->out_nodes[i].lock());
                }
            }
            rpath.pop_back();
            rpaths.push_back(rpath);
            rpath.clear();
        }
    }
    return rpaths;
}

float KmerGraphWithCoverage::prob_path(const std::vector<KmerNodePtr> &kpath,
                           const uint32_t &sample_id) {
    float ret_p = 0;
    for (uint32_t i = 0; i != kpath.size(); ++i) {
        ret_p += prob(kpath[i]->id, sample_id);
    }
    uint32_t len = kpath.size();
    if (kpath[0]->path.length() == 0) {
        len -= 1;
    }
    if (kpath.back()->path.length() == 0) {
        len -= 1;
    }
    if (len == 0) {
        len = 1;
    }
    return ret_p / len;
}

/*
float KmerGraph::prob_paths(const std::vector<std::vector<KmerNodePtr>> &kpaths) {
    if (kpaths.empty()) {
        return 0; // is this the correct default?
    }

    // collect how much coverage we expect on each node from these paths
    std::vector<uint32_t> path_node_covg(nodes.size(), 0);
    for (uint32_t i = 0; i != kpaths.size(); ++i) {
        for (uint32_t j = 0; j != kpaths[i].size(); ++j) {
            path_node_covg[kpaths[i][j]->id] += 1;
        }
    }

    // now calculate max likelihood assuming independent paths
    float ret_p = 0;
    uint32_t len = 0;
    for (uint32_t i = 0; i != path_node_covg.size(); ++i) {
        if (path_node_covg[i] > 0) {
            //cout << "prob of node " << nodes[i]->id << " which has path covg " << path_node_covg[i] << " and so we expect to see " << num_reads*path_node_covg[i]/kpaths.size() << " times IS " << prob(nodes[i]->id, num_reads*path_node_covg[i]/kpaths.size()) << endl;
                ret_p += prob(nodes[i]->id, num_reads * path_node_covg[i] / kpaths.size(), <#initializer#>);
            if (nodes[i]->path.length() > 0) {
                len += 1;
            }
        }
    }

    if (len == 0) {
        len = 1;
    }

    //cout << "len " << len << endl;
    return ret_p / len;
}
*/

void KmerGraphWithCoverage::save_covg_dist(const std::string &filepath) {
    std::ofstream handle;
    handle.open(filepath);

    for (const auto &kmer_node_ptr: kmer_prg->nodes) {
        const KmerNode &kmer_node = *kmer_node_ptr;

        uint32_t sample_id = 0;
        for (const auto &sample_coverage: nodeIndex2SampleCoverage[kmer_node.id]) {
            handle << kmer_node.id << " "
                   << sample_id << " "
                   << sample_coverage.first << " "
                   << sample_coverage.second;

            sample_id++;
        }
    }
    handle.close();
}

//save the KmerGraph as gfa
//TODO: THIS SHOULD BE RECODED, WE ARE DUPLICATING CODE HERE (SEE KmerGraph::save())!!!
void KmerGraphWithCoverage::save(const std::string &filepath, const std::shared_ptr<LocalPRG> localprg) {
    uint32_t sample_id = 0;

    std::ofstream handle;
    handle.open(filepath);
    if (handle.is_open()) {
        handle << "H\tVN:Z:1.0\tbn:Z:--linear --singlearr" << std::endl;
        for (const auto &c : kmer_prg->nodes) {
            handle << "S\t" << c->id << "\t";

            if (localprg != nullptr) {
                handle << localprg->string_along_path(c->path);
            } else {
                handle << c->path;
            }

            handle << "\tFC:i:" << get_covg(c->id, 0, sample_id) << "\t" << "\tRC:i:"
                   << get_covg(c->id, 1, sample_id) << std::endl;//"\t" << (unsigned)nodes[i].second->num_AT << endl;

            for (uint32_t j = 0; j < c->out_nodes.size(); ++j) {
                handle << "L\t" << c->id << "\t+\t" << c->out_nodes[j].lock()->id << "\t+\t0M" << std::endl;
            }
        }
        handle.close();
    } else {
        std::cerr << "Unable to open kmergraph file " << filepath << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

//TODO: THIS SHOULD BE RECODED, WE ARE DUPLICATING CODE HERE (SEE KmerGraph::load())!!!
void KmerGraphWithCoverage::load(const std::string &filepath) {
    //TODO: this might be dangerous, recode this?
    auto kmer_prg = const_cast<KmerGraph*>(this->kmer_prg);
    kmer_prg->clear();
    uint32_t sample_id = 0;

    std::string line;
    std::vector<std::string> split_line;
    std::stringstream ss;
    uint32_t id = 0, covg, from, to;
    prg::Path p;
    uint32_t num_nodes = 0;

    std::ifstream myfile(filepath);
    if (myfile.is_open()) {

        while (getline(myfile, line).good()) {
            if (line[0] == 'S') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 4);
                id = std::stoi(split_line[1]);
                num_nodes = std::max(num_nodes, id);
            }
        }
        myfile.clear();
        myfile.seekg(0, myfile.beg);
        kmer_prg->nodes.reserve(num_nodes);
        std::vector<uint16_t> outnode_counts(num_nodes + 1, 0), innode_counts(num_nodes + 1, 0);

        while (getline(myfile, line).good()) {
            if (line[0] == 'S') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 4);
                id = stoi(split_line[1]);
                ss << split_line[2];
                char c = ss.peek();
                assert (isdigit(c) or assert_msg("Cannot read in this sort of kmergraph GFA as it does not label nodes "
                                                 "with their PRG path"));
                ss >> p;
                ss.clear();
                //add_node(p);
                KmerNodePtr n = std::make_shared<KmerNode>(id, p);
                assert(n != nullptr);
                assert(id == kmer_prg->nodes.size() or num_nodes - id == kmer_prg->nodes.size() or
                       assert_msg("id " << id << " != " << kmer_prg->nodes.size() << " nodes.size() for kmergraph "));
                kmer_prg->nodes.push_back(n);
                kmer_prg->sorted_nodes.insert(n);
                if (kmer_prg->k == 0 and p.length() > 0) {
                    kmer_prg->k = p.length();
                }
                covg = stoi(split(split_line[3], "FC:i:")[0]);
                set_covg(n->id, covg, 0, sample_id);
                covg = stoi(split(split_line[4], "RC:i:")[0]);
                set_covg(n->id, covg, 1, sample_id);
                if (split_line.size() >= 6) {
                    n->num_AT = std::stoi(split_line[5]);
                }
            } else if (line[0] == 'L') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 5);
                assert(stoi(split_line[1]) < (int) outnode_counts.size() or
                       assert_msg(stoi(split_line[1]) << ">=" << outnode_counts.size()));
                assert(stoi(split_line[3]) < (int) innode_counts.size() or
                       assert_msg(stoi(split_line[3]) << ">=" << innode_counts.size()));
                outnode_counts[stoi(split_line[1])] += 1;
                innode_counts[stoi(split_line[3])] += 1;
            }
        }

        if (id == 0) {
            reverse(kmer_prg->nodes.begin(), kmer_prg->nodes.end());
        }

        id = 0;
        for (const auto &n : kmer_prg->nodes) {
            assert(kmer_prg->nodes[id]->id == id);
            id++;
            assert(n->id < outnode_counts.size() or assert_msg(n->id << ">=" << outnode_counts.size()));
            assert(n->id < innode_counts.size() or assert_msg(n->id << ">=" << innode_counts.size()));
            n->out_nodes.reserve(outnode_counts[n->id]);
            n->in_nodes.reserve(innode_counts[n->id]);
        }

        myfile.clear();
        myfile.seekg(0, myfile.beg);

        while (getline(myfile, line).good()) {
            if (line[0] == 'L') {
                split_line = split(line, "\t");
                assert(split_line.size() >= 5);
                if (split_line[2] == split_line[4]) {
                    from = std::stoi(split_line[1]);
                    to = std::stoi(split_line[3]);
                } else {
                    // never happens
                    from = std::stoi(split_line[3]);
                    to = std::stoi(split_line[1]);
                }
                kmer_prg->add_edge(kmer_prg->nodes[from], kmer_prg->nodes[to]);
                //nodes[from]->outNodes.push_back(nodes.at(to));
                //nodes[to]->inNodes.push_back(nodes.at(from));
            }
        }
    } else {
        std::cerr << "Unable to open kmergraph file " << filepath << std::endl;
        exit(1);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//KmerGraph methods definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////