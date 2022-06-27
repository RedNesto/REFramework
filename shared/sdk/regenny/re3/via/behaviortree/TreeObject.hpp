#pragma once
#include "ManagedObjectArray.hpp"
#include "NodeArray.hpp"
namespace regenny::via::behaviortree {
struct TreeObjectData;
}
namespace regenny::via::behaviortree {
struct TreeNode;
}
namespace regenny::via::behaviortree {
#pragma pack(push, 1)
struct TreeObject {
    void* vfptr; // 0x0
    regenny::via::behaviortree::TreeObjectData* data; // 0x8
    regenny::via::behaviortree::NodeArray nodes; // 0x10
    char pad_1c[0x4];
    regenny::via::behaviortree::ManagedObjectArray selectors; // 0x20
    char pad_30[0x10];
    regenny::via::behaviortree::ManagedObjectArray actions; // 0x40
    regenny::via::behaviortree::ManagedObjectArray conditions; // 0x50
    regenny::via::behaviortree::ManagedObjectArray transitions; // 0x60
    regenny::via::behaviortree::ManagedObjectArray expression_tree_conditions; // 0x70
    char pad_80[0x10];
    regenny::via::behaviortree::ManagedObjectArray delayed_actions; // 0x90
    regenny::via::behaviortree::TreeNode* root_node; // 0xa0
    char pad_a8[0x30];
}; // Size: 0xd8
#pragma pack(pop)
}