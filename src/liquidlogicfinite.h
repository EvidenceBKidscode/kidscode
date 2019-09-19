/*
Minetest
Copyright (C) 2019 Pierre-Yves Rollo <dev@pyrollo.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "liquidlogic.h"
#include "util/container.h"
#include "irrlichttypes_bloated.h"
#include "mapnode.h"

class ServerEnvironment;
class IGameDef;
class Map;
class MapBlock;
class MapNode;

struct LiquidInfo {
	content_t c_source;
	content_t c_flowing;
	content_t c_empty;
	// Slide specific
	content_t c_solid;
	u8 blocks;
	std::string break_group;
	std::string liquify_group;
	std::string stop_group;
};

struct NodeInfo {
	v3s16 pos;
	s8 level;
	s8 space;
	MapNode node;
	bool flowing_down;
	bool wet;
};

struct FlowInfo {
	s8 in = 0;
	s8 out = 0;
	content_t c_liquid_source = CONTENT_IGNORE;
};

class LiquidLogicFinite: public LiquidLogic {
public:
	LiquidLogicFinite(Map *map, IGameDef *gamedef);
	void addTransforming(v3s16 p);
	void scanBlock(MapBlock *block);
	void scanVoxelManip(MMVManip *vm, v3s16 nmin, v3s16 nmax);
	void scanVoxelManip(UniqueQueue<v3s16> *liquid_queue,
		MMVManip *vm, v3s16 nmin, v3s16 nmax);
	void transform(std::map<v3s16, MapBlock*> &modified_blocks,
		ServerEnvironment *env);
	void addTransformingFromData(BlockMakeData *data);

private:
	u8 get_group(content_t c_node, const std::string &group_name);
	u8 get_group(MapNode node, const std::string &group_name);
	LiquidInfo get_liquid_info(v3s16 pos);
	LiquidInfo get_liquid_info(content_t c_node);
	NodeInfo get_node_info(v3s16 pos, const LiquidInfo &liquid);
	void set_node(v3s16 pos, MapNode node,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
	void add_flow(v3s16 pos, s8 amount, const LiquidInfo &liquid);
	s8 transfer(NodeInfo &source, NodeInfo &target,
		const LiquidInfo &liquid, bool equalize);
	void compute_flow(v3s16 pos);
	void apply_flow(v3s16 pos, FlowInfo flow,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);

	// slides specific methods
	FlowInfo neighboor_flow(v3s16 pos, const LiquidInfo &liquid);
	u8 evaluate_neighboor_liquid(v3s16 pos, const LiquidInfo &liquid);
	u8 count_neighboor_with_group(v3s16 pos, std::string group);
	void solidify(NodeInfo &info, const LiquidInfo &liquid,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
	bool try_liquify(v3s16 pos, const LiquidInfo &liquid,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
	void transform_slide(v3s16 pos, FlowInfo &flow,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
/*
	void update_node(NodeInfo &info, const LiquidInfo &liquid,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
	void solidify(NodeInfo &info, const LiquidInfo &liquid,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
	void try_liquidify(v3s16 pos, const LiquidInfo &liquid,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
	void try_break(v3s16 pos, s8 transfer, const LiquidInfo &liquid,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
	void liquify_and_break(NodeInfo &info, s8 transfer, const LiquidInfo &liquid,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);
*/

	UniqueQueue<v3s16> m_liquid_queue;
	std::unordered_map<u64, FlowInfo> m_flows;
	std::vector<std::pair<v3s16, MapNode> > m_changed_nodes;
	v3s16 m_block_pos, m_rel_block_pos;

	// Cached node def informations
	std::unordered_map<content_t, LiquidInfo> m_liquids_info;
	std::map<std::string, std::map<content_t, u8>> m_groups_info;

	u32 m_unprocessed_count = 0;
	bool m_queue_size_timer_started = false;
	u64 m_inc_trending_up_start_time = 0; // milliseconds
};
