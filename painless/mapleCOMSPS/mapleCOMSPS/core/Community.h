#pragma once

#include <vector>
using namespace std;

namespace MapleCOMSPS {

class Community {
   vector<int> m_var2node;
   vector<int> m_node2area;
   bool louvain_done;

public:
   Community() {
      louvain_done = false;
   };

   Community(const Community&) = delete;

   ~Community() {};

   bool ready() { return louvain_done; }

   int var2node(int var);

   int node2area(int node);

   vector<int>& get_var2node();

   vector<int>& get_node2area();

   void set_ready(bool b);
};

inline int Community::var2node(int var) { return this->m_var2node[var]; }

inline int Community::node2area(int node) { return this->m_node2area[node]; }

}