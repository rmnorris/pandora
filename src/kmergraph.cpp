#include <iostream>
#include <cstring>
#include <fstream>
#include <cassert>
#include <vector>
#include "utils.h"
#include "kmernode.h"
#include "kmergraph.h"

#define assert_msg(x) !(std::cerr << "Assertion failed: " << x << std::endl)

using namespace std;

KmerGraph::KmerGraph()
{
    nodes.reserve(5000);
    next_id = 0;
}

KmerGraph::~KmerGraph()
{
  for (auto c: nodes)
  {
    delete c;
  }
}

void KmerGraph::add_node (const Path& p)
{
    KmerNode *n;
    n = new KmerNode(next_id, p);
    pointer_values_equal<KmerNode> eq = { n };
    if ( find_if(nodes.begin(), nodes.end(), eq) == nodes.end() )
    {
	nodes.push_back(n);
	next_id++;
    } else {
	delete n;
    }
    return;
}
void KmerGraph::add_edge (const uint32_t& from, const uint32_t& to)
{
    assert(from <= nodes.size() && to <= nodes.size());
    pointer_values_equal<KmerNode> eq = { nodes[to] };
    if ( find_if(nodes[from]->outNodes.begin(), nodes[from]->outNodes.end(), eq) == nodes[from]->outNodes.end() )
    {
        nodes[from]->outNodes.push_back(nodes[to]);
    }
    eq = { nodes[from] };
    if ( find_if(nodes[to]->inNodes.begin(), nodes[to]->inNodes.end(), eq) == nodes[to]->inNodes.end() )
    {
        nodes[to]->inNodes.push_back(nodes[from]);
    }
    return;
}

condition::condition(const Path& p): q(p) {};
bool condition::operator()(const KmerNode* kn) const { return kn->path == q; }

void KmerGraph::add_edge (const Path& from, const Path& to)
{
    vector<KmerNode*>::iterator from_it = find_if(nodes.begin(), nodes.end(), condition(from));
    vector<KmerNode*>::iterator to_it = find_if(nodes.begin(), nodes.end(), condition(to));
    assert(from_it != nodes.end() && to_it != nodes.end());

    pointer_values_equal<KmerNode> eq_t = { *to_it };
    if ( find_if((*from_it)->outNodes.begin(), (*from_it)->outNodes.end(), eq_t) == (*from_it)->outNodes.end() )
    {
        (*from_it)->outNodes.push_back(*to_it);
    }
    pointer_values_equal<KmerNode> eq_f = { *from_it };
    if ( find_if((*to_it)->inNodes.begin(), (*to_it)->inNodes.end(), eq_f) == (*to_it)->inNodes.end() )
    {
        (*to_it)->inNodes.push_back((*from_it));
    }
    return;
}

void KmerGraph::save (const string& filepath)
{
    ofstream handle;
    handle.open (filepath);
    handle.close();
}

void KmerGraph::load (const string& filepath)
{
    string line;

    ifstream myfile (filepath);
    if (myfile.is_open())
    {
        while ( getline (myfile,line).good() )
        {
	}

        myfile.clear();
    } else {
        cerr << "Unable to open kmergraph file " << filepath << endl;
        exit(1);
    }
    return;		
}

bool KmerGraph::operator == (const KmerGraph& y) const
{
    // false if have different numbers of nodes
    if (y.nodes.size() != nodes.size()) {//cout << "different numbers of nodes" << endl; 
        return false;}

    // false if have different nodes
    for ( const auto c: nodes)
    {
        // if node not equal to a node in y, then false
        pointer_values_equal<KmerNode> eq = { c };
        if ( find_if(y.nodes.begin(), y.nodes.end(), eq) == y.nodes.end() )
	{
            return false;
	}
    }
    // otherwise is true
    return true;
}

std::ostream& operator<< (std::ostream & out, KmerGraph const& data) {
    for (const auto c: data.nodes)
    {
        out << *c;
    }
    return out ;
}
