#include "Community.h"

using namespace MapleCOMSPS;

vector<int>& Community::get_var2node()
{
	return m_var2node;
}

vector<int>& Community::get_node2area()
{
	return m_node2area;
}

void Community::set_ready(bool b)
{
	louvain_done = b;
}
