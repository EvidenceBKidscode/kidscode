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

#include "liquidlogicpreserve.h"
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

const v3s16 side_4dirs[4] =
{
	v3s16( 0, 0, 1),
	v3s16( 1, 0, 0),
	v3s16( 0, 0,-1),
	v3s16(-1, 0, 0)
};

const v3s16 down_dir = v3s16( 0,-1, 0);

LiquidLogicPreserve::LiquidLogicPreserve(Map *map, IGameDef *gamedef) :
	LiquidLogic(map, gamedef)
{
}

void LiquidLogicPreserve::addTransforming(v3s16 p) {
	m_liquid_queue.push_back(p);
}

void LiquidLogicPreserve::addTransformingFromData(BlockMakeData *data)
{
	while (data->transforming_liquid.size()) {
		m_liquid_queue.push_back(data->transforming_liquid.front());
		data->transforming_liquid.pop_front();
	}
}

void LiquidLogicPreserve::scanBlock(MapBlock *block)
{
	// Very basic scan: pushes all liquid blocks with PRESERVE logic
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

void LiquidLogicPreserve::scanVoxelManip(UniqueQueue<v3s16> *liquid_queue,
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

void LiquidLogicPreserve::scanVoxelManip(MMVManip *vm, v3s16 nmin, v3s16 nmax)
{
	scanVoxelManip(&m_liquid_queue, vm, nmin, nmax);
}

LiquidInfo LiquidLogicPreserve::get_liquid_info(v3s16 pos) {
	LiquidInfo info;
	MapNode node = m_map->getNodeNoEx(pos);
	const ContentFeatures &cf = m_ndef->get(node);
	info.c_source = m_ndef->getId(cf.liquid_alternative_source);
	info.c_flowing = m_ndef->getId(cf.liquid_alternative_flowing);
	info.c_empty = CONTENT_AIR;
	return info;
}

NodeInfo LiquidLogicPreserve::get_node_info(v3s16 pos, LiquidInfo liquid) {
	NodeInfo info;
	info.pos = pos;
	info.node = m_map->getNodeNoEx(pos);
	info.level = -1; // By default, not fillable
	info.fillable = false; // Can hold more liquid

	if (info.node.getContent() == CONTENT_IGNORE) return info;

	const ContentFeatures &cf = m_ndef->get(info.node);

	switch (cf.liquid_type) {
		case LIQUID_SOURCE:
			if (m_ndef->getId(cf.liquid_alternative_source) == liquid.c_source)
				info.level =  LIQUID_LEVEL_SOURCE;
			break;
		case LIQUID_FLOWING:
			if (m_ndef->getId(cf.liquid_alternative_source) == liquid.c_source) {
				info.level = info.node.param2 & LIQUID_LEVEL_MASK;
				info.fillable = true;
			}
			break;
		case LIQUID_NONE:
			if (cf.floodable) {
				info.level = 0;
				info.fillable = true;
			}
			break;
	}

	return info;
}

void LiquidLogicPreserve::update_node(NodeInfo &info, LiquidInfo liquid,
	std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env) {
	NodeInfo old = info;
	if (info.level >= LIQUID_LEVEL_SOURCE) {
			info.node.setContent(liquid.c_source);
			info.node.param2 = 0;
	} else if (info.level <= 0) {
			info.node.setContent(liquid.c_empty);
			info.node.param2 = 0;
		} else {
			info.node.setContent(liquid.c_flowing);
			info.node.param2 = (info.flowing_down ? LIQUID_FLOW_DOWN_MASK : 0x00)
				| (info.level & LIQUID_LEVEL_MASK);
		}

	// No change, skip
	if (old.node.getContent() == info.node.getContent()
	 && old.node.param2 == info.node.param2)
		return;

	// Node has changed this turn, ignore it
	m_skip.insert(info.pos);

	// But check it next turn
	m_must_reflow.push_back(info.pos);

	old.level = (old.node.getContent() == liquid.c_source)?LIQUID_LEVEL_SOURCE:
		old.node.param2&LIQUID_LEVEL_MASK; // Supose param2=0 for empty nodes

	// Trigger liquid transform on side blocks if level has lowered
	if (old.level > info.level) {
		for (u16 i = 0; i < 4; i++)
			m_must_reflow.push_back(info.pos + side_4dirs[i]);
		m_must_reflow.push_back(info.pos - down_dir);
	}

	// on_flood() the node
	const ContentFeatures &cf = m_ndef->get(old.node);
	if (old.node.getContent() != CONTENT_AIR && cf.liquid_type == LIQUID_NONE) {
		if (env->getScriptIface()->node_on_flood(info.pos, old.node, info.node))
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
		m_changed_nodes.emplace_back(info.pos, old.node);
	}
}

void LiquidLogicPreserve::distribute(
	std::list<NodeInfo> targets, NodeInfo &source,
	LiquidInfo liquid, bool equalize,
	std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env
)
{
	do
	{
		std::list<NodeInfo>::iterator target = targets.begin();
		while (target != targets.end())
		{
			if (target->level >= LIQUID_LEVEL_SOURCE || source.level <= (equalize?1:0)
				|| (equalize && (target->level >= source.level))) {
				update_node(*target, liquid, modified_blocks, env);
				targets.erase(target++);
				continue;
			}
			if (source.level > 0)
			{
				target->level++;
				source.level--;
			}
			target++;
		}
	} while (!targets.empty());
}

void LiquidLogicPreserve::transform(
	std::map<v3s16, MapBlock*> &modified_blocks,
	ServerEnvironment *env)
{
	u32 loopcount = 0;
	u32 initial_size = m_liquid_queue.size();
	u16 start = rand()%4;
	NodeInfo info;

	m_must_reflow.clear();
	m_changed_nodes.clear();
	m_skip.clear();

	u32 liquid_loop_max = g_settings->getS32("liquid_loop_max");
	u32 loop_max = liquid_loop_max;

	while (m_liquid_queue.size() != 0)
	{
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size || loopcount >= loop_max)
			break;
		loopcount++;

		/*
			Get a queued transforming liquid node
		*/
		v3s16 pos = m_liquid_queue.front();

		m_liquid_queue.pop_front();
		if (m_skip.count(pos) > 0) continue;

		// Get source node information
		LiquidInfo liquid = get_liquid_info(pos);
		NodeInfo source = get_node_info(pos, liquid);
		if (source.level <= 0) continue;

		// Blocks to fill in priority :
		// 1 - Block under
		// 2 - Blocks under flowable neighboors
		// 3 - Side blocks

		// Right under (1)
		info = get_node_info(pos + down_dir, liquid);
		if (info.fillable) {
			distribute(std::list<NodeInfo>{info}, source, liquid, false, modified_blocks, env);
			if (source.level <= 0) {
				update_node(source, liquid, modified_blocks, env);
				continue;
			}
		}

		// Side blocks and blocks under (2+3)
		std::list<NodeInfo> sides;
		std::list<NodeInfo> under;

		for (u16 i = 0; i < 4; i++) {
			v3s16 tgtpos = pos + side_4dirs[(i + start)%4];
			info = get_node_info(tgtpos, liquid);
			if (info.fillable) {
				sides.push_back(info);
				info = get_node_info(tgtpos + down_dir, liquid);
				if (info.fillable)
					under.push_back(info);
			}
		}
		distribute(under, source, liquid, false, modified_blocks, env);
		if (source.level <= 0) {
			update_node(source, liquid, modified_blocks, env);
			continue;
		}
		distribute(sides, source, liquid, true, modified_blocks, env);
		update_node(source, liquid, modified_blocks, env);
	} // while (m_liquid_queue.size() != 0)

	//infostream<<"Map::transformLiquids(): loopcount="<<loopcount<<std::endl;

	for (auto &iter : m_must_reflow)
		m_liquid_queue.push_back(iter);

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
