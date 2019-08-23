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

#include "liquidlogicfinite.h"
#include "map.h"
#include "mapblock.h"
#include "nodedef.h"
#include "porting.h"
#include "serverenvironment.h"
#include "script/scripting_server.h"
#include "rollback_interface.h"
#include "gamedef.h"
#include "voxelalgorithms.h"
#include "emerge.h"
#include <algorithm>

const v3s16 dbg_pos(-655, 1331, 929);

u32 dbg_solid = 0;
u32 dbg_liquid = 0;

const v3s16 side_4dirs[4] =
{
	v3s16( 0, 0, 1),
	v3s16( 1, 0, 0),
	v3s16( 0, 0,-1),
	v3s16(-1, 0, 0)
};

const v3s16 down_dir = v3s16( 0,-1, 0);

union pos_and_key
{
	s16 pos[4];
	u64 key;
};

u64 pos_to_key(v3s16 pos)
{
	pos_and_key u;

	u.pos[0] = pos.X;
	u.pos[1] = pos.Y;
	u.pos[2] = pos.Z;
	u.pos[3] = 0;
	return u.key;
}

v3s16 key_to_pos(u64 key)
{
	pos_and_key u;
	u.key = key;
	return v3s16(u.pos[0], u.pos[1], u.pos[2]);
}

/*
u64 pos_to_key(v3s16 pos)
{
	s16 key16[4];
	key16[0] = pos.X;
	key16[1] = pos.Y;
	key16[2] = pos.Z;
	key16[3] = 0;

	return *((u64 *)key16);
}

v3s16 key_to_pos(u64 key)
{
	s16 *key16 = *key;
	return v3s16(key16[0], key16[1], key16[2]);
}
*/
LiquidLogicFinite::LiquidLogicFinite(Map *map, IGameDef *gamedef) :
	LiquidLogic(map, gamedef)
{
}


void LiquidLogicFinite::addTransforming(v3s16 p) {
	m_liquid_queue.push_back(p);
}

void LiquidLogicFinite::addTransformingFromData(BlockMakeData *data)
{
	while (data->transforming_liquid.size()) {
		m_liquid_queue.push_back(data->transforming_liquid.front());
		data->transforming_liquid.pop_front();
	}
}

void LiquidLogicFinite::scanBlock(MapBlock *block)
{
	// Very basic scan: pushes all liquid blocks with finite logic
	// The transfom method will do the job
	bool valid_position;

	m_block_pos = block->getPos();
	m_rel_block_pos = block->getPosRelative();

	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
	for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
		MapNode node = block->getNodeNoCheck(x, y, z, &valid_position);
		if (m_ndef->get(node).isLiquid())
			m_liquid_queue.push_back(m_rel_block_pos + v3s16(x, y, z));
	}
}

void LiquidLogicFinite::scanVoxelManip(UniqueQueue<v3s16> *liquid_queue,
	MMVManip *vm, v3s16 nmin, v3s16 nmax)
{
	for (s16 z = nmin.Z + 1; z <= nmax.Z - 1; z++)
	for (s16 y = nmax.Y; y >= nmin.Y; y--) {
		u32 vi = vm->m_area.index(nmin.X + 1, y, z);
		for (s16 x = nmin.X + 1; x <= nmax.X - 1; x++) {
			if (vm->m_data[vi].getContent() != CONTENT_IGNORE &&
				m_ndef->get(vm->m_data[vi]).isLiquid())
				liquid_queue->push_back(v3s16(x, y, z));
			vi++;
		}
	}
}

void LiquidLogicFinite::scanVoxelManip(MMVManip *vm, v3s16 nmin, v3s16 nmax)
{
	scanVoxelManip(&m_liquid_queue, vm, nmin, nmax);
}

void LiquidLogicFinite::add_flow(v3s16 pos, s8 amount,
		const LiquidInfo &liquid)
{
	u64 key = pos_to_key(pos);
	FlowInfo flow = m_flows[key];
	if (flow.c_liquid_source != CONTENT_IGNORE &&
			flow.c_liquid_source != liquid.c_source) {
		printf("%d %d %d Flows cannot mix liquids %d and %d.\n",
				pos.X, pos.Y, pos.Z, liquid.c_source, flow.c_liquid_source);
		return;
	}
	flow.c_liquid_source = liquid.c_source;
	flow.amount += amount;
	m_flows[key] = flow;
}

LiquidInfo LiquidLogicFinite::get_liquid_info(content_t c_node) {
	try {
		return m_liquids_info.at(c_node);
	} catch(std::out_of_range &e) {}

	LiquidInfo info;

	const ContentFeatures &cf = m_ndef->get(c_node);
	info.c_source = m_ndef->getId(cf.liquid_alternative_source);
	try {
		info = m_liquids_info.at(info.c_source);
		m_liquids_info[c_node] = info;
		return info;
	} catch(std::out_of_range &e) {}

	if (info.c_source != CONTENT_IGNORE) {
		info.c_flowing = m_ndef->getId(cf.liquid_alternative_flowing);
		info.c_solid = m_ndef->getId(cf.liquid_alternative_solid);
		info.c_empty = CONTENT_AIR;
	} else {
		info.c_flowing = CONTENT_IGNORE;
		info.c_solid = CONTENT_IGNORE;
		info.c_empty = CONTENT_AIR;
	}

	if (info.c_solid != CONTENT_IGNORE) {
		info.blocks = cf.liquid_blocks_per_solid;
		info.break_group = "broken_by_" + cf.liquid_slide_type_name ;
		info.liquify_group = "liquified_by_" + cf.liquid_slide_type_name;
	} else {
		info.blocks = 0;
		info.liquify_group = "";
		info.break_group = "";
	}

	m_liquids_info[c_node] = info;

	return info;
}

LiquidInfo LiquidLogicFinite::get_liquid_info(v3s16 pos) {
	MapNode node = m_map->getNodeNoEx(pos);
	return get_liquid_info(node.getContent());
}

// TODO: Could be improved by caching some node info in flows (level, space)
NodeInfo LiquidLogicFinite::get_node_info(v3s16 pos, const LiquidInfo &liquid) {
	NodeInfo info;
	info.pos = pos;
	info.node = m_map->getNodeNoEx(pos);
	info.level = -1; // By default, not fillable
	info.space = 0;
	info.wet = false; // Is wet (flowing, including lvl 0 or source)

	if (info.node.getContent() == CONTENT_IGNORE) return info;

	const ContentFeatures &cf = m_ndef->get(info.node);

	switch (cf.liquid_type) {
		case LIQUID_SOURCE:
			if (m_ndef->getId(cf.liquid_alternative_source) == liquid.c_source) {
				info.level = LIQUID_LEVEL_SOURCE;
				info.wet = true;
			}
			break;
		case LIQUID_FLOWING:
			if (m_ndef->getId(cf.liquid_alternative_source) == liquid.c_source) {
				info.level = info.node.param2 & LIQUID_LEVEL_MASK;
				info.wet = true;
			}
			break;
		case LIQUID_NONE:
			if (cf.floodable) {
				info.level = 0;
			}
			break;
	}

	// If liquid or fillable, test existing pending flow
	if (info.level >= 0) {
		FlowInfo flow;
		try {
			flow = m_flows.at(pos_to_key(pos));
		} catch(std::out_of_range &e) {
			info.space = LIQUID_LEVEL_SOURCE - info.level;
			return info;
		}

		if (flow.c_liquid_source != liquid.c_source) {
		 	if (info.level > 0) {
				// Should never occur Could be an exception
				printf("%d, %d, %d : mixed liquids %d and %d\n",
					pos.X, pos.Y, pos.Z, liquid.c_source, flow.c_liquid_source);
			}
			info.level = -1; // Cannot fill, already filled with another liquid.
			info.space = 0;
			info.wet = false; // Not sure of this one
			return info;
		}

		info.level += flow.amount;
		if (info.level > LIQUID_LEVEL_SOURCE) {
			printf("%d, %d, %d : overflow (level=%d, flow=%d)\n",
					pos.X, pos.Y, pos.Z, info.level, flow.amount);
			info.level = LIQUID_LEVEL_SOURCE;
		} else if (info.level < 0) {
			printf("%d, %d, %d : underflow (level=%d, flow=%d)\n",
					pos.X, pos.Y, pos.Z, info.level, flow.amount);
			info.level = 0;
		}
		info.space = LIQUID_LEVEL_SOURCE - info.level;
	}

	return info;
}
/*
u8 LiquidLogicFinite::evaluate_neighboor_liquid(v3s16 pos, const LiquidInfo &liquid)
{
	u8 volume = 0;
	MapNode node;
	for (s16 X = pos.X - 1; X <= pos.X + 1; X++)
	for (s16 Y = pos.Y - 1; Y <= pos.Y + 1; Y++)
	for (s16 Z = pos.Z - 1; Z <= pos.Z + 1; Z++)
		if (X || Y || Z)
		{
			node = m_map->getNodeNoEx(v3s16(X, Y, Z));
			if (node.getContent() == liquid.c_source)
				volume += LIQUID_LEVEL_SOURCE;
			else if (node.getContent() == liquid.c_flowing)
				volume += node.param2 & LIQUID_LEVEL_MASK;
		}
	return volume;
}

u8 LiquidLogicFinite::count_neighboor_with_group(v3s16 pos, std::string group)
{
	u8 count = 0;
	MapNode node;

	for (s16 X = pos.X - 1; X <= pos.X + 1; X++)
	for (s16 Y = pos.Y - 1; Y <= pos.Y + 1; Y++)
	for (s16 Z = pos.Z - 1; Z <= pos.Z + 1; Z++)
		if (X || Y || Z)
		{
			node = m_map->getNodeNoEx(v3s16(X, Y, Z));
			if (m_ndef->get(node).getGroup(group) > 0)
				count++;
		}
	return count;
}
*/
/*
// Slide specific method
void LiquidLogicFinite::solidify(NodeInfo &info, const LiquidInfo &liquid,
	std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env)
{
	// Does this liquid solidifies ?
	if (liquid.c_solid == CONTENT_IGNORE)
		return;

	MapNode nunder = m_map->getNodeNoEx(info.pos + down_dir);

	// Never solidify above CONTENT_IGNORE
	if (nunder.getContent() == CONTENT_IGNORE ||
			nunder.getContent() == CONTENT_UNKNOWN)
		return;

	// Never solidify above a liquid
	const ContentFeatures &cf = m_ndef->get(nunder);
	if (cf.isLiquid())
		return;

	if ((std::rand() % (8 * liquid.blocks)) < info.level) {
		info.level = -2;
		dbg_solid++;
	} else {
		info.level = -1;
	}

	update_node(info, liquid, modified_blocks, env);
}

void LiquidLogicFinite::try_liquidify(v3s16 pos, const LiquidInfo &liquid,
	std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env)
{
	const ContentFeatures &cf = m_ndef->get(m_map->getNodeNoEx(pos));
	if (cf.getGroup(liquid.liquify_group) == 0)
		return;

	NodeInfo info;

	if (liquid.blocks > 1) {
		s8 space_needed = (liquid.blocks - 1) << 3;
		std::vector<NodeInfo> spaces;
		// Try to find space below
		info = get_node_info(pos + down_dir, liquid);
		if (info.space)
			spaces.push_back(info);
		space_needed -= info.space;

		// Then around
		if (space_needed > 0) {
			u8 start = std::rand() % 4;
			u8 i = 0;
			while (space_needed > 0 && i < 4) {
				info = get_node_info(pos + side_4dirs[(i + start)%4], liquid);
				if (info.space)
					spaces.push_back(info);
				space_needed -= info.space;
				i++;
			}
		}

		if (space_needed > 0)
			return; // Failed to expand solid node to enough liquid nodes

		space_needed = (liquid.blocks - 1) << 3;
		for (auto &info : spaces) {
			s8 fill = info.space > space_needed ? space_needed : info.space;
			info.level += fill;
			update_node(info, liquid, modified_blocks, env);
			space_needed -= fill;
		}
	}
	//printf("%d, %d, %d :liquify\n",pos.X, pos.Y, pos.Z);

	dbg_liquid++;
	info = get_node_info(pos, liquid);
	info.level = LIQUID_LEVEL_SOURCE;
	update_node(info, liquid, modified_blocks, env);
}


const v3s16 liquify_dirs[5] =
{
	v3s16( 0, -1,  0),
	v3s16( 0,  0,  1),
	v3s16( 1,  0,  0),
	v3s16( 0,  0, -1),
	v3s16(-1,  0,  0)
};

void LiquidLogicFinite::try_break(v3s16 pos, s8 transfer,
	const LiquidInfo &liquid, std::map<v3s16, MapBlock*> &modified_blocks,
	ServerEnvironment *env)
{
	NodeInfo info = get_node_info(pos, liquid);
	u8 group = m_ndef->get(info.node).getGroup(liquid.break_group);
	if (group and std::rand() % 500 < group * transfer) {
		info.level = -1;
		update_node(info, liquid, modified_blocks, env);
	}
}

// Slide specific method
void LiquidLogicFinite::liquify_and_break(NodeInfo &info, s8 transfer,
	const LiquidInfo &liquid, std::map<v3s16, MapBlock*> &modified_blocks,
	ServerEnvironment *env)
{
	u16 test = transfer * evaluate_neighboor_liquid(info.pos, liquid);

	for (int i = 0; i < 5; i++) {
		if (std::rand() % 300 < test)
			try_liquidify(info.pos + liquify_dirs[i], liquid, modified_blocks, env);
		try_break(info.pos + liquify_dirs[i], transfer, liquid, modified_blocks, env);
	}
}
*/

s8 LiquidLogicFinite::transfer(NodeInfo &source, NodeInfo &target,
	const LiquidInfo &liquid, bool equalize)
{
	s8 transfer;
	if (equalize)
		transfer = (source.level - target.level + 1) / 2;
	else
		transfer = source.level;

	if (target.level + transfer > LIQUID_LEVEL_SOURCE)
		transfer = LIQUID_LEVEL_SOURCE - target.level;

	if (transfer <= 0) return 0;

	target.level+= transfer;
	source.level-= transfer;

	add_flow(source.pos, -transfer, liquid);
	add_flow(target.pos,  transfer, liquid);

	return transfer;
}

void LiquidLogicFinite::compute_flow(v3s16 pos)
{
	// Get source node information
	LiquidInfo liquid = get_liquid_info(pos);
	NodeInfo source = get_node_info(pos, liquid);

	if (!source.wet) return;

	// Level 0 nodes have to be checked every turn for drying
	if (!source.level) {
		add_flow(source.pos, 0, liquid);
		return;
	}

	NodeInfo info;
	/*
	if ((liquid.c_solid != CONTENT_IGNORE) &&
			(std::rand()%6 > evaluate_neighboor_liquid(pos, liquid))) {
			solidify(source, liquid, modified_blocks, env);
			return;
		}
*/
	// Blocks to fill in priority :
	// 1 - Block under
	// 2 - Blocks under flowable neighboors
	// 3 - Side blocks

	// Right under (1)
	info = get_node_info(pos + down_dir, liquid);

	if (info.space) {
		transfer(source, info, liquid, false);
		if (source.level <= 0)
			return;
	}

	// Find side blocks and blocks under (2+3)
	std::vector<NodeInfo> sides;
	std::vector<NodeInfo> under;
	u8 start = std::rand() % 4;

	for (u16 i = 0; i < 4; i++) {
		v3s16 tgtpos = pos + side_4dirs[(i + start)%4];
		info = get_node_info(tgtpos, liquid);
		if (info.space) {
			sides.push_back(info);
			info = get_node_info(tgtpos + down_dir, liquid);
			if (info.space)
				under.push_back(info);
		}
	}

	// Start with lowest liquid level first
	struct {
		bool operator()(NodeInfo a, NodeInfo b) const
		{	return a.level < b.level; }
	} levelCompare;

	std::sort(under.begin(), under.end(), levelCompare);
	std::sort(sides.begin(), sides.end(), levelCompare);

	// Distribute under (2)
	// First distribute to liquids
	for (auto& target : under)
		if (source.level > 0 && target.wet)
			if (transfer(source, target, liquid, false))
				return;

	// Then to others
	for (auto& target : under)
		if (source.level > 0 && !target.wet)
			if (transfer(source, target, liquid, false))
				return;

	// Distribute to sides
	// First distribute to liquids
	for (auto& target : sides)
		if (source.level > 0 && target.wet)
			if (transfer(source, target, liquid, true))
				return;

	// Then to others
	for (auto& target : sides)
		if (source.level > 1 && !target.wet)
			transfer(source, target, liquid, true);
}

void LiquidLogicFinite::apply_flow(v3s16 pos, FlowInfo flow,
	std::map<v3s16, MapBlock*> &modified_blocks,
	ServerEnvironment *env)
{
	LiquidInfo liquid = get_liquid_info(flow.c_liquid_source);
	NodeInfo info = get_node_info(pos, liquid);
	MapNode nold = info.node;

	switch(info.level) {
		case LIQUID_LEVEL_SOURCE:
			info.node.setContent(liquid.c_source);
			info.node.param2 = 0;
			break;

		// Level 0 nodes are kept "wet" for some times before being dried. This is
		// used to have a realistic looking liquid path
		case 0:
			info.node.param2 = 0;
			info.node.setContent(liquid.c_empty);

			// Keep wet if neighboors are wet
			for (u16 i = 0; i < 4; i++) {
				MapNode node = m_map->getNodeNoEx(pos + side_4dirs[i]);
				if (node.getContent() == liquid.c_source ||
						(node.getContent() == liquid.c_flowing &&
						node.param2 > 0))
				{
					info.node.setContent(liquid.c_flowing);
					break;
				}
			}

			// Sometimes, a wet node gets dried
			if (info.node.getContent() == liquid.c_flowing && (std::rand() % 10 == 0))
				info.node.setContent(liquid.c_empty);
			break;

		default:
			info.node.setContent(liquid.c_flowing);
			info.node.param2 = info.level & LIQUID_LEVEL_MASK;
	}

	// Put liquid nodes that level changed back to queue
	if (info.node.getContent() == liquid.c_flowing ||
			info.node.getContent() == liquid.c_source)
		m_liquid_queue.push_back(pos);

	// No change on map, skip
	if (nold.getContent() == info.node.getContent()
	 && nold.param2 == info.node.param2)
		return;

	// Trigger liquid transform on side blocks if level has lowered
	if (flow.amount < 0) {
		for (u16 i = 0; i < 4; i++)
			m_liquid_queue.push_back(info.pos + side_4dirs[i]);
		m_liquid_queue.push_back(info.pos - down_dir);
	}

	// on_flood() the node
	// TODO: Check this, not sure of triggering conds
	const ContentFeatures &cf = m_ndef->get(nold);
	if (nold.getContent() != CONTENT_AIR && cf.liquid_type == LIQUID_NONE) {
		if (env->getScriptIface()->node_on_flood(info.pos, nold, info.node))
			return;
	}

	// Ignore light (because calling voxalgo::update_lighting_nodes)
	info.node.setLight(LIGHTBANK_DAY, 0, m_ndef);
	info.node.setLight(LIGHTBANK_NIGHT, 0, m_ndef);

	// Find out whether there is a suspect for this action
	std::string suspect;
	if (m_gamedef->rollback())
		suspect = m_gamedef->rollback()->getSuspect(info.pos, 83, 1);

	if (m_gamedef->rollback() && !suspect.empty()) {
		// Blame suspect
		RollbackScopeActor rollback_scope(m_gamedef->rollback(), suspect, true);
		// Get old node for rollback
		RollbackNode rollback_oldnode(m_map, info.pos, m_gamedef);
		// Set node
		m_map->setNode(info.pos, info.node);
		// Report
		RollbackNode rollback_newnode(m_map, info.pos, m_gamedef);
		RollbackAction action;
		action.setSetNode(info.pos, rollback_oldnode, rollback_newnode);
		m_gamedef->rollback()->reportAction(action);
	} else {
		// Set node
		m_map->setNode(info.pos, info.node);
	}

	v3s16 blockpos = getNodeBlockPos(info.pos);
	MapBlock *block = m_map->getBlockNoCreateNoEx(blockpos);
	if (block != NULL) {
		modified_blocks[blockpos] =  block;
		m_changed_nodes.emplace_back(info.pos, nold);
	}
}

void LiquidLogicFinite::transform(
	std::map<v3s16, MapBlock*> &modified_blocks,
	ServerEnvironment *env)
{
	u32 loopcount = 0;
	u32 initial_size = m_liquid_queue.size();
	NodeInfo info;

	m_changed_nodes.clear();
	m_flows.clear();

	u32 liquid_loop_max = g_settings->getS32("liquid_loop_max");
	u32 loop_max = liquid_loop_max;

//	printf("Liquid queue size = %d\n", m_liquid_queue.size());

	// First compute flows from nodes to others
	while (m_liquid_queue.size() != 0) {
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size || loopcount >= loop_max)
			break;
		loopcount++;

		v3s16 pos = m_liquid_queue.front();
		m_liquid_queue.pop_front();
		compute_flow(pos);
	}
//	printf("Pending flow size = %ld\n", m_flows.size());

	// Then apply flows. This will populate m_liquid_queue also for the next run
	for (auto &it: m_flows) {
		apply_flow(key_to_pos(it.first), it.second, modified_blocks, env);
	}

//	printf("liquid %d, solid %d, delta %d\n", dbg_liquid, dbg_solid, dbg_liquid - dbg_solid);

	//infostream<<"Map::transformLiquids(): loopcount="<<loopcount<<std::endl;
/* --> inutile, remplacé par apply_flow()
	for (auto &iter : m_must_reflow)
		m_liquid_queue.push_back(iter);
*/
	voxalgo::update_lighting_nodes(m_map, m_changed_nodes, modified_blocks);

	/* ----------------------------------------------------------------------
	 * Manage the queue so that it does not grow indefinately
	 */
	u16 time_until_purge = g_settings->getU16("liquid_queue_purge_time");

	if (time_until_purge == 0)
		return; // Feature disabled

	time_until_purge *= 1000;	// seconds -> milliseconds

	u64 curr_time = porting::getTimeMs();
	u32 prev_unprocessed = m_unprocessed_count;
	m_unprocessed_count = m_liquid_queue.size();

	// if unprocessed block count is decreasing or stable
	if (m_unprocessed_count <= prev_unprocessed) {
		m_queue_size_timer_started = false;
	} else {
		if (!m_queue_size_timer_started)
			m_inc_trending_up_start_time = curr_time;
		m_queue_size_timer_started = true;
	}

	// Account for curr_time overflowing
	if (m_queue_size_timer_started && m_inc_trending_up_start_time > curr_time)
		m_queue_size_timer_started = false;

	/* If the queue has been growing for more than liquid_queue_purge_time seconds
	 * and the number of unprocessed blocks is still > liquid_loop_max then we
	 * cannot keep up; dump the oldest blocks from the queue so that the queue
	 * has liquid_loop_max items in it
	 */
	if (m_queue_size_timer_started
			&& curr_time - m_inc_trending_up_start_time > time_until_purge
			&& m_unprocessed_count > liquid_loop_max) {

		size_t dump_qty = m_unprocessed_count - liquid_loop_max;

		infostream << "transformLiquids(): DUMPING " << dump_qty
		           << " blocks from the queue" << std::endl;

		while (dump_qty--)
			m_liquid_queue.pop_front();

		m_queue_size_timer_started = false; // optimistically assume we can keep up now
		m_unprocessed_count = m_liquid_queue.size();
	}
}

/* LEVEL 0 MANAGEMENT
	// Level 0 flowing nodes are "wet" nodes. They make liquids follow the same
	// path and avoid scattering.

	// For level 0 flowing nodes, check if neighboors are level > 1
	if (source.level == 0) {
		source.level = -1;
		for (u16 i = 0; i < 4; i++) {
			v3s16 tgtpos = pos + side_4dirs[i];
			info = get_node_info(tgtpos, liquid);
			if (info.level > 0) {
				m_must_reflow.push_back(source.pos);
				source.level = 0;
				break;
			}
		}

		if (source.level == 0 && std::rand() % 10 == 0)
			source.level = -1;

		if (source.level == -1)
			update_node(source, liquid, modified_blocks, env);
		else
			// Don't remove node from queue until it is solidified (if it can).
			if (liquid.c_solid != CONTENT_IGNORE)
				m_must_reflow.push_back(source.pos);

		return;
	}
*/

/* SOLIDIFY
	if (liquid.c_solid != CONTENT_IGNORE) {
		if (std::rand() % 3 > transfered)
			solidify(source, liquid, modified_blocks, env);
		else
			// Don't remove node from queue until it is solidified.
			m_must_reflow.push_back(source.pos);
	}
*/
