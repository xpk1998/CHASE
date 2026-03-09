#pragma once

#include "interfaces/crypto/CommonType.h"
#include "FixedBytes.h"
#include "Ranges.h"

namespace bcos::crypto
{
template <class NodeType>
concept Node = requires(NodeType node)
{
    node->data();
};
template <class NodesType>
concept Nodes = requires(NodesType nodesType)
{
    requires RANGES::bidirectional_range<NodesType>;
    requires Node<RANGES::range_value_t<NodesType>>;
};
class KeyCompareTools
{
public:
    static bool compareTwoNodeIDs(Nodes auto const& nodes1, Nodes auto const& nodes2)
    {
        if (RANGES::size(nodes1) != RANGES::size(nodes2))
        {
            return false;
        }

        for (auto const& [idx, value] : nodes1 | RANGES::views::enumerate)
        {
            if (nodes2[idx]->data() != value->data())
            {
                return false;
            }
        }
        return true;
    }

    static bool isNodeIDExist(Node auto const& node, Nodes auto const& nodes)
    {
        return RANGES::find_if(RANGES::begin(nodes), RANGES::end(nodes),
                   [&node](auto&& n) { return n->data() == node->data(); }) != RANGES::end(nodes);
    }
};
}  // namespace bcos::crypto
