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
//#include "util/timetaker.h"
#include "serverenvironment.h"
#include "script/scripting_server.h"
#include "rollback_interface.h"
#include "gamedef.h"
#include "voxelalgorithms.h"


void LiquidLogicPreserve::addTransformingFromData(BlockMakeData *data)
{}


const v3s16 side_4dirs[4] =
{
	v3s16( 0, 0, 1), // back
	v3s16( 1, 0, 0), // right
	v3s16( 0, 0,-1), // front
	v3s16(-1, 0, 0) // left
};

const v3s16 down_dir = v3s16( 0,-1, 0);

LiquidLogicPreserve::LiquidLogicPreserve(Map *map, IGameDef *gamedef) :
	LiquidLogic(map, gamedef)
{
}

void LiquidLogicPreserve::addTransforming(v3s16 p) {
	m_liquid_queue.push_back(p);
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

// TOOD: should be inline
void LiquidLogicPreserve::setNodeLevel(
	MapNode &n, s8 l, bool flowing_down,
	content_t c_source, content_t c_flowing, content_t c_empty)
{
	if (l >= LIQUID_LEVEL_SOURCE) {
		n.setContent(c_source);
		n.param2 = 0;
	} else if (l <= 0) {
		n.setContent(c_empty);
		n.param2 = 0;
	} else {
		n.setContent(c_flowing);
		n.param2 = (flowing_down ? LIQUID_FLOW_DOWN_MASK : 0x00) | (l & LIQUID_LEVEL_MASK);
	}
}

s8 LiquidLogicPreserve::getNodeLevel(
	MapNode &n, content_t c_source)
{
	const ContentFeatures &cf = m_ndef->get(n);
	switch (cf.liquid_type) {
		case LIQUID_SOURCE:
			if (m_ndef->getId(cf.liquid_alternative_source) == c_source)
				return LIQUID_LEVEL_SOURCE;
			break;
		case LIQUID_FLOWING:
			if (m_ndef->getId(cf.liquid_alternative_source) == c_source)
				return (n.param2 & LIQUID_LEVEL_MASK);
			break;
		case LIQUID_NONE:
			if (cf.floodable)
				return 0;
			break;
	}
	return -1;
}

void LiquidLogicPreserve::updateNodeIfChanged(v3s16 pos, MapNode nnew, MapNode nold,
	std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env)
{
	if (nnew.getContent() == nold.getContent() && nnew.param2 == nold.param2)
		return;

	// on_flood() the node
	const ContentFeatures &cf = m_ndef->get(nold);
	if (nold.getContent() != CONTENT_AIR && cf.liquid_type == LIQUID_NONE) {
		if (env->getScriptIface()->node_on_flood(pos, nold, nnew))
			return;
	}

	// Ignore light (because calling voxalgo::update_lighting_nodes)
	nnew.setLight(LIGHTBANK_DAY, 0, m_ndef);
	nnew.setLight(LIGHTBANK_NIGHT, 0, m_ndef);

	// Find out whether there is a suspect for this action
	std::string suspect;
	if (m_gamedef->rollback())
		suspect = m_gamedef->rollback()->getSuspect(pos, 83, 1);

	if (m_gamedef->rollback() && !suspect.empty()) {
		// Blame suspect
		RollbackScopeActor rollback_scope(m_gamedef->rollback(), suspect, true);
		// Get old node for rollback
		RollbackNode rollback_oldnode(m_map, pos, m_gamedef);
		// Set node
		m_map->setNode(pos, nnew);
		// Report
		RollbackNode rollback_newnode(m_map, pos, m_gamedef);
		RollbackAction action;
		action.setSetNode(pos, rollback_oldnode, rollback_newnode);
		m_gamedef->rollback()->reportAction(action);
	} else {
		// Set node
		m_map->setNode(pos, nnew);
	}

	m_must_reflow.push_back(pos);

	v3s16 blockpos = getNodeBlockPos(pos);
	MapBlock *block = m_map->getBlockNoCreateNoEx(blockpos);
	if (block != NULL) {
		modified_blocks[blockpos] =  block;
		m_changed_nodes.emplace_back(pos, nold);
	}
}

void LiquidLogicPreserve::transform(
	std::map<v3s16, MapBlock*> &modified_blocks,
	ServerEnvironment *env)
{
	u32 loopcount = 0;
	u32 initial_size = m_liquid_queue.size();
	u16 start = rand()*4;

	m_must_reflow.clear();
	m_changed_nodes.clear();

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
		v3s16 p0 = m_liquid_queue.front();
		m_liquid_queue.pop_front();

		// Get source node information
		MapNode n0 = m_map->getNodeNoEx(p0);
		MapNode n00 = n0;

		s8 source_level = -1;
		const ContentFeatures &cf = m_ndef->get(n0);

		switch (cf.liquid_type) {
			case LIQUID_SOURCE:
				source_level = LIQUID_LEVEL_SOURCE;
				break;
			case LIQUID_FLOWING:
				source_level = (n0.param2 & LIQUID_LEVEL_MASK);
				break;
			case LIQUID_NONE:
				continue;
				break;
		}

		content_t c_flowing = m_ndef->getId(cf.liquid_alternative_flowing);
		content_t c_source = m_ndef->getId(cf.liquid_alternative_source);
		content_t c_empty = CONTENT_AIR;

		// Flow down
		v3s16 npos = p0 + down_dir;
		MapNode nb = m_map->getNodeNoEx(npos);
		s8 nb_level = getNodeLevel(nb, c_source);

		if (nb_level >= 0) {
			s8 transfer = LIQUID_LEVEL_SOURCE - nb_level;
			if (source_level < transfer) transfer = source_level;
			if (transfer > 0) {
				source_level = source_level - transfer;
				nb_level = nb_level + transfer;

				// Update target node
				MapNode nb0 = nb;
				setNodeLevel(nb, nb_level, false, c_source, c_flowing, c_empty);
				updateNodeIfChanged(npos, nb, nb0, modified_blocks, env);
			}
		}

		// Check source not empty
		if (source_level <= 0) {
			setNodeLevel(n0, source_level, true, c_source, c_flowing, c_empty);
			updateNodeIfChanged(p0, n0, n00, modified_blocks, env);

			// Source emptied, surrounding nodes may reflow
			m_must_reflow.push_back(p0 - down_dir);
			m_must_reflow.push_back(p0 + side_4dirs[0]);
			m_must_reflow.push_back(p0 + side_4dirs[1]);
			m_must_reflow.push_back(p0 + side_4dirs[2]);
			m_must_reflow.push_back(p0 + side_4dirs[3]);

			// Done with this node
			continue;
		}

		// Side blocks
		MapNode nbs[4];
		MapNode nbs_old[4];
		MapNode nbs_below[4];
		v3s16 nbs_pos[4];
		s8 nbs_level[4];

		//TODO: ADD RANDOM START
		for (u16 i = 0; i < 4; i++) {
			nbs_pos[i] = p0 + side_4dirs[i];
			nbs[i] = m_map->getNodeNoEx(nbs_pos[i]);
			nbs_old[i] = nbs[i];
			nbs_below[i].setContent(CONTENT_IGNORE);
			nbs_level[i] = getNodeLevel(nbs[i], c_source);

			// eliminate target already filled or higher than source
			if (nbs_level[i] >= source_level &&
				nbs_level[i] >= LIQUID_LEVEL_SOURCE)
				nbs_level[i] = -1;
		}

		u8 remaining;
		start++; start%=65000;
		do
		{
			remaining = 0;
			for (u16 j = 0; j < 4; j++)
			{
				u16 i = (start+j)%4;
				if (nbs_level[i] >= 0)
				{
					remaining ++;
					if (nbs_level[i] >= LIQUID_LEVEL_SOURCE ||
						nbs_level[i] >= source_level ||
						source_level <= 0) {
						setNodeLevel(nbs[i], nbs_level[i], false, c_source, c_flowing, c_empty);
						updateNodeIfChanged(nbs_pos[i], nbs[i], nbs_old[i], modified_blocks, env);

						nbs_level[i] = -1;
					}
					else
					{
						if (source_level > 1)
						{
							nbs_level[i]++;
							source_level--;
						}
						else
						{
							// Move level 1 only if it can fall above target
							// This is a atempt to avoid level 1 nodes moving around

							if (nbs_below[i].getContent() == CONTENT_IGNORE)
							 	nbs_below[i] = m_map->getNodeNoEx(nbs_pos[i] + down_dir);

							if (getNodeLevel(nbs_below[i], c_source) >= 0)
							{
								nbs_level[i]++;
								source_level--;
							}
							else
							{
								setNodeLevel(nbs[i], nbs_level[i], false, c_source, c_flowing, c_empty);
								updateNodeIfChanged(nbs_pos[i], nbs[i], nbs_old[i], modified_blocks, env);
								nbs_level[i] = -1;
							}
						}
					}
				}
			}
		} while (remaining);

		// Finally update source
		// Set level
		setNodeLevel(n0, source_level, (nb_level >= 0), c_source, c_flowing, c_empty);
		if (n0.getContent() != n00.getContent() || n0.param2 != n00.param2) {
			updateNodeIfChanged(p0, n0, n00, modified_blocks, env);
			// Source emptied, surrounding nodes may reflow
			m_must_reflow.push_back(p0 - down_dir);
			m_must_reflow.push_back(p0 + side_4dirs[0]);
			m_must_reflow.push_back(p0 + side_4dirs[1]);
			m_must_reflow.push_back(p0 + side_4dirs[2]);
			m_must_reflow.push_back(p0 + side_4dirs[3]);
		}
	}
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
