/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "map.h"
#include "mapsector.h"
#include "mapblock.h"
#include "filesys.h"
#include "voxel.h"
#include "voxelalgorithms.h"
#include "porting.h"
#include "serialization.h"
#include "nodemetadata.h"
#include "settings.h"
#include "log.h"
#include "profiler.h"
#include "nodedef.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "util/basic_macros.h"
#include "rollback_interface.h"
#include "environment.h"
#include "reflowscan.h"
#include "emerge.h"
#include "mapgen/mapgen_v6.h"
#include "mapgen/mg_biome.h"
#include "config.h"
#include "server.h"
#include "database/database.h"
#include "database/database-dummy.h"
#include "database/database-sqlite3.h"
#include "script/scripting_server.h"
#include "liquidlogic.h"
#include "liquidlogicclassic.h"
//#include "liquidlogicpreserve.h"
#include "liquidlogicfinite.h"
#include <deque>
#include <queue>
#if USE_LEVELDB
#include "database/database-leveldb.h"
#endif
#if USE_REDIS
#include "database/database-redis.h"
#endif
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif


/*
	Map
*/

Map::Map(std::ostream &dout, IGameDef *gamedef):
	m_dout(dout),
	m_gamedef(gamedef),
	m_nodedef(gamedef->ndef())
{
	m_liquid_logic = new LiquidLogicFinite(this, gamedef);
//	m_liquid_logic = new LiquidLogicPreserve(this, gamedef);
//	m_liquid_logic = new LiquidLogicClassic(this, gamedef);
}

Map::~Map()
{
	/*
		Free all MapSectors
	*/
	for (auto &sector : m_sectors) {
		delete sector.second;
	}

	delete m_liquid_logic;
}

void Map::addEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.insert(event_receiver);
}

void Map::removeEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.erase(event_receiver);
}

void Map::dispatchEvent(const MapEditEvent &event)
{
	for (MapEventReceiver *event_receiver : m_event_receivers) {
		event_receiver->onMapEditEvent(event);
	}
}

MapSector * Map::getSectorNoGenerateNoLock(v2s16 p)
{
	if(m_sector_cache != NULL && p == m_sector_cache_p){
		MapSector * sector = m_sector_cache;
		return sector;
	}

	std::map<v2s16, MapSector*>::iterator n = m_sectors.find(p);

	if (n == m_sectors.end())
		return NULL;

	MapSector *sector = n->second;

	// Cache the last result
	m_sector_cache_p = p;
	m_sector_cache = sector;

	return sector;
}

MapSector * Map::getSectorNoGenerate(v2s16 p)
{
	return getSectorNoGenerateNoLock(p);
}

MapBlock * Map::getBlockNoCreateNoEx(v3s16 p3d)
{
	v2s16 p2d(p3d.X, p3d.Z);
	MapSector * sector = getSectorNoGenerate(p2d);
	if(sector == NULL)
		return NULL;
	MapBlock *block = sector->getBlockNoCreateNoEx(p3d.Y);
	return block;
}

MapBlock * Map::getBlockNoCreate(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if(block == NULL)
		throw InvalidPositionException();
	return block;
}

bool Map::isNodeUnderground(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	return block && block->getIsUnderground();
}

bool Map::isValidPosition(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	return (block != NULL);
}

// Returns a CONTENT_IGNORE node if not found
MapNode Map::getNode(v3s16 p, bool *is_valid_position)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block == NULL) {
		if (is_valid_position != NULL)
			*is_valid_position = false;
		return {CONTENT_IGNORE};
	}

	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	bool is_valid_p;
	MapNode node = block->getNodeNoCheck(relpos, &is_valid_p);
	if (is_valid_position != NULL)
		*is_valid_position = is_valid_p;
	return node;
}

// throws InvalidPositionException if not found
void Map::setNode(v3s16 p, MapNode & n)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	// Never allow placing CONTENT_IGNORE, it causes problems
	if(n.getContent() == CONTENT_IGNORE){
		bool temp_bool;
		errorstream<<"Map::setNode(): Not allowing to place CONTENT_IGNORE"
				<<" while trying to replace \""
				<<m_nodedef->get(block->getNodeNoCheck(relpos, &temp_bool)).name
				<<"\" at "<<PP(p)<<" (block "<<PP(blockpos)<<")"<<std::endl;
		return;
	}
	block->setNodeNoCheck(relpos, n);
}

void Map::addNodeAndUpdate(v3s16 p, MapNode n,
		std::map<v3s16, MapBlock*> &modified_blocks,
		bool remove_metadata)
{
	// Collect old node for rollback
	RollbackNode rollback_oldnode(this, p, m_gamedef);

	// This is needed for updating the lighting
	MapNode oldnode = getNode(p);

	// Remove node metadata
	if (remove_metadata) {
		removeNodeMetadata(p);
	}

	// Set the node on the map
	// Ignore light (because calling voxalgo::update_lighting_nodes)
	n.setLight(LIGHTBANK_DAY, 0, m_nodedef);
	n.setLight(LIGHTBANK_NIGHT, 0, m_nodedef);
	setNode(p, n);

	// Update lighting
	std::vector<std::pair<v3s16, MapNode> > oldnodes;
	oldnodes.emplace_back(p, oldnode);
	voxalgo::update_lighting_nodes(this, oldnodes, modified_blocks);

	for (auto &modified_block : modified_blocks) {
		modified_block.second->expireDayNightDiff();
	}

	// Report for rollback
	if(m_gamedef->rollback())
	{
		RollbackNode rollback_newnode(this, p, m_gamedef);
		RollbackAction action;
		action.setSetNode(p, rollback_oldnode, rollback_newnode);
		m_gamedef->rollback()->reportAction(action);
	}

	/*
		Add neighboring liquid nodes and this node to transform queue.
		(it's vital for the node itself to get updated last, if it was removed.)
	 */

	for (const v3s16 &dir : g_7dirs) {
		v3s16 p2 = p + dir;

		bool is_valid_position;
		MapNode n2 = getNode(p2, &is_valid_position);
		if(is_valid_position &&
				(m_nodedef->get(n2).isLiquid() ||
				n2.getContent() == CONTENT_AIR))
			transforming_liquid_add(p2);
	}

}

void Map::removeNodeAndUpdate(v3s16 p,
		std::map<v3s16, MapBlock*> &modified_blocks)
{
	addNodeAndUpdate(p, MapNode(CONTENT_AIR), modified_blocks, true);
}

bool Map::addNodeWithEvent(v3s16 p, MapNode n, bool remove_metadata)
{
	MapEditEvent event;
	event.type = remove_metadata ? MEET_ADDNODE : MEET_SWAPNODE;
	event.p = p;
	event.n = n;

	bool succeeded = true;
	try{
		std::map<v3s16, MapBlock*> modified_blocks;
		addNodeAndUpdate(p, n, modified_blocks, remove_metadata);

		// Copy modified_blocks to event
		for (auto &modified_block : modified_blocks) {
			event.modified_blocks.insert(modified_block.first);
		}
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(event);

	return succeeded;
}

bool Map::removeNodeWithEvent(v3s16 p)
{
	MapEditEvent event;
	event.type = MEET_REMOVENODE;
	event.p = p;

	bool succeeded = true;
	try{
		std::map<v3s16, MapBlock*> modified_blocks;
		removeNodeAndUpdate(p, modified_blocks);

		// Copy modified_blocks to event
		for (auto &modified_block : modified_blocks) {
			event.modified_blocks.insert(modified_block.first);
		}
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(event);

	return succeeded;
}

// >> KIDSCODE - Threading (moved to clientmap.cpp and ServerMap)
/*
struct TimeOrderedMapBlock {
	MapSector *sect;
	MapBlock *block;

	TimeOrderedMapBlock(MapSector *sect, MapBlock *block) :
		sect(sect),
		block(block)
	{}

	bool operator<(const TimeOrderedMapBlock &b) const
	{
		return block->getUsageTimer() < b.block->getUsageTimer();
	};
};

/ *
	Updates usage timers
* /
void Map::timerUpdate(float dtime, float unload_timeout, u32 max_loaded_blocks,
		std::vector<v3s16> *unloaded_blocks)
{
	bool save_before_unloading = (mapType() == MAPTYPE_SERVER);

	// Profile modified reasons
	Profiler modprofiler;

	std::vector<v2s16> sector_deletion_queue;
	u32 deleted_blocks_count = 0;
	u32 saved_blocks_count = 0;
	u32 block_count_all = 0;

	beginSave();

	// If there is no practical limit, we spare creation of mapblock_queue
	if (max_loaded_blocks == U32_MAX) {
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			bool all_blocks_deleted = true;

			MapBlockVect blocks;
			sector->getBlocks(blocks);

			for (MapBlock *block : blocks) {
				block->incrementUsageTimer(dtime);

				if (block->refGet() == 0
						&& block->getUsageTimer() > unload_timeout) {
					v3s16 p = block->getPos();

					// Save if modified
					if (block->getModified() != MOD_STATE_CLEAN
							&& save_before_unloading) {
						modprofiler.add(block->getModifiedReasonString(), 1);
						if (!saveBlock(block))
							continue;
						saved_blocks_count++;
					}

					// Delete from memory
					sector->deleteBlock(block);

					if (unloaded_blocks)
						unloaded_blocks->push_back(p);

					deleted_blocks_count++;
				} else {
					all_blocks_deleted = false;
					block_count_all++;
				}
			}

			if (all_blocks_deleted) {
				sector_deletion_queue.push_back(sector_it.first);
			}
		}
	} else {
		std::priority_queue<TimeOrderedMapBlock> mapblock_queue;
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			MapBlockVect blocks;
			sector->getBlocks(blocks);

			for (MapBlock *block : blocks) {
				block->incrementUsageTimer(dtime);
				mapblock_queue.push(TimeOrderedMapBlock(sector, block));
			}
		}
		block_count_all = mapblock_queue.size();
		// Delete old blocks, and blocks over the limit from the memory
		while (!mapblock_queue.empty() && (mapblock_queue.size() > max_loaded_blocks
				|| mapblock_queue.top().block->getUsageTimer() > unload_timeout)) {
			TimeOrderedMapBlock b = mapblock_queue.top();
			mapblock_queue.pop();

			MapBlock *block = b.block;

			if (block->refGet() != 0)
				continue;

			v3s16 p = block->getPos();

			// Save if modified
			if (block->getModified() != MOD_STATE_CLEAN && save_before_unloading) {
				modprofiler.add(block->getModifiedReasonString(), 1);
				if (!saveBlock(block))
					continue;
				saved_blocks_count++;
			}

			// Delete from memory
			b.sect->deleteBlock(block);

			if (unloaded_blocks)
				unloaded_blocks->push_back(p);

			deleted_blocks_count++;
			block_count_all--;
		}
		// Delete empty sectors
		for (auto &sector_it : m_sectors) {
			if (sector_it.second->empty()) {
				sector_deletion_queue.push_back(sector_it.first);
			}
		}
	}
	endSave();

	// Finally delete the empty sectors
	deleteSectors(sector_deletion_queue);

	if(deleted_blocks_count != 0)
	{
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Unloaded "<<deleted_blocks_count
				<<" blocks from memory";
		if(save_before_unloading)
			infostream<<", of which "<<saved_blocks_count<<" were written";
		infostream<<", "<<block_count_all<<" blocks in memory";
		infostream<<"."<<std::endl;
		if(saved_blocks_count != 0){
			PrintInfo(infostream); // ServerMap/ClientMap:
			infostream<<"Blocks modified by: "<<std::endl;
			modprofiler.print(infostream);
		}
	}
}

void Map::unloadUnreferencedBlocks(std::vector<v3s16> *unloaded_blocks)
{
	timerUpdate(0.0, -1.0, 0, unloaded_blocks);
}
*/
// << KIDSCODE - Threading

void Map::deleteSectors(std::vector<v2s16> &sectorList)
{
	for (v2s16 j : sectorList) {
		MapSector *sector = m_sectors[j];
		// If sector is in sector cache, remove it from there
		if(m_sector_cache == sector)
			m_sector_cache = NULL;
		// Remove from map and delete
		m_sectors.erase(j);
		delete sector;
	}
}

void Map::PrintInfo(std::ostream &out)
{
	out<<"Map: ";
}

void Map::transforming_liquid_add(v3s16 p) {
	m_liquid_logic->addTransforming(p);
}

// >> KIDSCODE - Threading - Moved to ServerMap
/*
void Map::enableLiquidsTransform(bool enabled) {
	m_liquid_transform_enabled = enabled;
}

void Map::transformLiquids(std::map<v3s16, MapBlock*> &modified_blocks,
		ServerEnvironment *env)
{
	if (m_liquid_transform_enabled)
		m_liquid_logic->transform(modified_blocks, env);
}
*/
// << KIDSCODE - Threading ?

std::vector<v3s16> Map::findNodesWithMetadata(v3s16 p1, v3s16 p2)
{
	std::vector<v3s16> positions_with_meta;

	sortBoxVerticies(p1, p2);
	v3s16 bpmin = getNodeBlockPos(p1);
	v3s16 bpmax = getNodeBlockPos(p2);

	VoxelArea area(p1, p2);

	for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
	for (s16 y = bpmin.Y; y <= bpmax.Y; y++)
	for (s16 x = bpmin.X; x <= bpmax.X; x++) {
		v3s16 blockpos(x, y, z);

		MapBlock *block = getBlockNoCreateNoEx(blockpos);
		if (!block) {
			verbosestream << "Map::getNodeMetadata(): Need to emerge "
				<< PP(blockpos) << std::endl;
			block = emergeBlock(blockpos, false);
		}
		if (!block) {
			infostream << "WARNING: Map::getNodeMetadata(): Block not found"
				<< std::endl;
			continue;
		}

		v3s16 p_base = blockpos * MAP_BLOCKSIZE;
		std::vector<v3s16> keys = block->m_node_metadata.getAllKeys();
		for (size_t i = 0; i != keys.size(); i++) {
			v3s16 p(keys[i] + p_base);
			if (!area.contains(p))
				continue;

			positions_with_meta.push_back(p);
		}
	}

	return positions_with_meta;
}

NodeMetadata *Map::getNodeMetadata(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::getNodeMetadata(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::getNodeMetadata(): Block not found"
				<<std::endl;
		return NULL;
	}
	NodeMetadata *meta = block->m_node_metadata.get(p_rel);
	return meta;
}

bool Map::setNodeMetadata(v3s16 p, NodeMetadata *meta)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::setNodeMetadata(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::setNodeMetadata(): Block not found"
				<<std::endl;
		return false;
	}
	block->m_node_metadata.set(p_rel, meta);
	return true;
}

void Map::removeNodeMetadata(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
	{
		warningstream<<"Map::removeNodeMetadata(): Block not found"
				<<std::endl;
		return;
	}
	block->m_node_metadata.remove(p_rel);
}

NodeTimer Map::getNodeTimer(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::getNodeTimer(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::getNodeTimer(): Block not found"
				<<std::endl;
		return NodeTimer();
	}
	NodeTimer t = block->m_node_timers.get(p_rel);
	NodeTimer nt(t.timeout, t.elapsed, p);
	return nt;
}

void Map::setNodeTimer(const NodeTimer &t)
{
	v3s16 p = t.position;
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::setNodeTimer(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::setNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	NodeTimer nt(t.timeout, t.elapsed, p_rel);
	block->m_node_timers.set(nt);
}

void Map::removeNodeTimer(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
	{
		warningstream<<"Map::removeNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	block->m_node_timers.remove(p_rel);
}

bool Map::determineAdditionalOcclusionCheck(const v3s16 &pos_camera,
	const core::aabbox3d<s16> &block_bounds, v3s16 &check)
{
	/*
		This functions determines the node inside the target block that is
		closest to the camera position. This increases the occlusion culling
		accuracy in straight and diagonal corridors.
		The returned position will be occlusion checked first in addition to the
		others (8 corners + center).
		No position is returned if
		- the closest node is a corner, corners are checked anyway.
		- the camera is inside the target block, it will never be occluded.
	*/
#define CLOSEST_EDGE(pos, bounds, axis) \
	((pos).axis <= (bounds).MinEdge.axis) ? (bounds).MinEdge.axis : \
	(bounds).MaxEdge.axis

	bool x_inside = (block_bounds.MinEdge.X <= pos_camera.X) &&
			(pos_camera.X <= block_bounds.MaxEdge.X);
	bool y_inside = (block_bounds.MinEdge.Y <= pos_camera.Y) &&
			(pos_camera.Y <= block_bounds.MaxEdge.Y);
	bool z_inside = (block_bounds.MinEdge.Z <= pos_camera.Z) &&
			(pos_camera.Z <= block_bounds.MaxEdge.Z);

	if (x_inside && y_inside && z_inside)
		return false; // Camera inside target mapblock

	// straight
	if (x_inside && y_inside) {
		check = v3s16(pos_camera.X, pos_camera.Y, 0);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (y_inside && z_inside) {
		check = v3s16(0, pos_camera.Y, pos_camera.Z);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		return true;
	} else if (x_inside && z_inside) {
		check = v3s16(pos_camera.X, 0, pos_camera.Z);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		return true;
	}

	// diagonal
	if (x_inside) {
		check = v3s16(pos_camera.X, 0, 0);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (y_inside) {
		check = v3s16(0, pos_camera.Y, 0);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (z_inside) {
		check = v3s16(0, 0, pos_camera.Z);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		return true;
	}

	// Closest node would be a corner, none returned
	return false;
}

bool Map::isOccluded(const v3s16 &pos_camera, const v3s16 &pos_target,
	float step, float stepfac, float offset, float end_offset, u32 needed_count)
{
	v3f direction = intToFloat(pos_target - pos_camera, BS);
	float distance = direction.getLength();

	// Normalize direction vector
	if (distance > 0.0f)
		direction /= distance;

	v3f pos_origin_f = intToFloat(pos_camera, BS);
	u32 count = 0;
	bool is_valid_position;

	for (; offset < distance + end_offset; offset += step) {
		v3f pos_node_f = pos_origin_f + direction * offset;
		v3s16 pos_node = floatToInt(pos_node_f, BS);

		MapNode node = getNode(pos_node, &is_valid_position);

		if (is_valid_position &&
				!m_nodedef->get(node).light_propagates) {
			// Cannot see through light-blocking nodes --> occluded
			count++;
			if (count >= needed_count)
				return true;
		}
		step *= stepfac;
	}
	return false;
}

bool Map::isBlockOccluded(MapBlock *block, v3s16 cam_pos_nodes)
{
	// Check occlusion for center and all 8 corners of the mapblock
	// Overshoot a little for less flickering
	static const s16 bs2 = MAP_BLOCKSIZE / 2 + 1;
	static const v3s16 dir9[9] = {
		v3s16( 0,  0,  0),
		v3s16( 1,  1,  1) * bs2,
		v3s16( 1,  1, -1) * bs2,
		v3s16( 1, -1,  1) * bs2,
		v3s16( 1, -1, -1) * bs2,
		v3s16(-1,  1,  1) * bs2,
		v3s16(-1,  1, -1) * bs2,
		v3s16(-1, -1,  1) * bs2,
		v3s16(-1, -1, -1) * bs2,
	};

	v3s16 pos_blockcenter = block->getPosRelative() + (MAP_BLOCKSIZE / 2);

	// Starting step size, value between 1m and sqrt(3)m
	float step = BS * 1.2f;
	// Multiply step by each iteraction by 'stepfac' to reduce checks in distance
	float stepfac = 1.05f;

	float start_offset = BS * 1.0f;

	// The occlusion search of 'isOccluded()' must stop short of the target
	// point by distance 'end_offset' to not enter the target mapblock.
	// For the 8 mapblock corners 'end_offset' must therefore be the maximum
	// diagonal of a mapblock, because we must consider all view angles.
	// sqrt(1^2 + 1^2 + 1^2) = 1.732
	float end_offset = -BS * MAP_BLOCKSIZE * 1.732f;

	// to reduce the likelihood of falsely occluded blocks
	// require at least two solid blocks
	// this is a HACK, we should think of a more precise algorithm
	u32 needed_count = 2;

	// Additional occlusion check, see comments in that function
	v3s16 check;
	if (determineAdditionalOcclusionCheck(cam_pos_nodes, block->getBox(), check)) {
		// node is always on a side facing the camera, end_offset can be lower
		if (!isOccluded(cam_pos_nodes, check, step, stepfac, start_offset,
				-1.0f, needed_count))
			return false;
	}

	for (const v3s16 &dir : dir9) {
		if (!isOccluded(cam_pos_nodes, pos_blockcenter + dir, step, stepfac,
				start_offset, end_offset, needed_count))
			return false;
	}
	return true;
}

// >> KIDSCODE - Threading

void ServerMapMutex::lock_exclusive() {
//	printf("ServerMapMutex %lx wants exclusive\n", std::this_thread::get_id());
	m_multiple_mutex.lock();
	m_exclusive_mutex.lock();
//	if (m_shared_count) printf("ServerMapMutex %lx waits for %d shared locks to be released\n", std::this_thread::get_id(), m_shared_count);
	while (m_shared_count) {
		m_exclusive_mutex.unlock();
		m_multiple_mutex.unlock();
		std::this_thread::yield();
		m_multiple_mutex.lock();
		m_exclusive_mutex.lock();
	}
	m_thread = std::this_thread::get_id();
//	printf("ServerMapMutex %lx got exclusive\n", std::this_thread::get_id());
}

void ServerMapMutex::unlock_exclusive() {
//	printf("ServerMapMutex %lx releases exclusive\n", std::this_thread::get_id());
	m_thread = std::thread::id();
	m_exclusive_mutex.unlock();
	m_multiple_mutex.unlock();
}

void ServerMapMutex::lock_single() {
//	printf("ServerMapMutex %lx     wants single\n", std::this_thread::get_id());
	if (m_thread == std::this_thread::get_id()) {
		m_shared_count++;
	} else {
		std::unique_lock<std::mutex> lock(m_exclusive_mutex);
		m_shared_count++;
	}
//	printf("ServerMapMutex %lx     got single\n", std::this_thread::get_id());
}

void ServerMapMutex::unlock_single() {
//	printf("ServerMapMutex %lx     releases single\n", std::this_thread::get_id());
	if (m_thread == std::this_thread::get_id()) {
		if (m_shared_count > 0)
			m_shared_count--;
	} else {
		std::unique_lock<std::mutex> lock(m_exclusive_mutex);
		if (m_shared_count > 0)
			m_shared_count--;
	}
}

void ServerMapMutex::lock_multiple() {
//	printf("ServerMapMutex %lx   wants multiple\n", std::this_thread::get_id());
	m_multiple_mutex.lock();
	std::unique_lock<std::mutex> lock(m_exclusive_mutex);
	m_shared_count++;
//	printf("ServerMapMutex %lx   got multiple\n", std::this_thread::get_id());
}

void ServerMapMutex::unlock_multiple() {
//	printf("ServerMapMutex %lx   releases multiple\n", std::this_thread::get_id());
	{
		std::unique_lock<std::mutex> lock(m_exclusive_mutex);
		if (m_shared_count > 0)
			m_shared_count--;
	}
	m_multiple_mutex.unlock();
}
// << KIDSCODE - Threading

/*
	ServerMap
*/
ServerMap::ServerMap(const std::string &savedir, IGameDef *gamedef,
		EmergeManager *emerge):
	Map(dout_server, gamedef),
	settings_mgr(g_settings, savedir + DIR_DELIM + "map_meta.txt"),
	m_emerge(emerge),
	m_map_save_thread(this) // KIDSCODE - Threading
{
	verbosestream<<FUNCTION_NAME<<std::endl;

	// Tell the EmergeManager about our MapSettingsManager
	emerge->map_settings_mgr = &settings_mgr;

	/*
		Try to load map; if not found, create a new one.
	*/

	// Determine which database backend to use
	std::string conf_path = savedir + DIR_DELIM + "world.mt";
	Settings conf;
	bool succeeded = conf.readConfigFile(conf_path.c_str());
	if (!succeeded || !conf.exists("backend")) {
		// fall back to sqlite3
		conf.set("backend", "sqlite3");
	}
	std::string backend = conf.get("backend");
	dbase = createDatabase(backend, savedir, conf);
	if (conf.exists("readonly_backend")) {
		std::string readonly_dir = savedir + DIR_DELIM + "readonly";
		dbase_ro = createDatabase(conf.get("readonly_backend"), readonly_dir, conf);
	}
	if (!conf.updateConfigFile(conf_path.c_str()))
		errorstream << "ServerMap::ServerMap(): Failed to update world.mt!" << std::endl;

	m_savedir = savedir;
	m_map_saving_enabled = false;

	try {
		// If directory exists, check contents and load if possible
		if (fs::PathExists(m_savedir)) {
			// If directory is empty, it is safe to save into it.
			if (fs::GetDirListing(m_savedir).empty()) {
				infostream<<"ServerMap: Empty save directory is valid."
						<<std::endl;
				m_map_saving_enabled = true;
			}
			else
			{

				if (settings_mgr.loadMapMeta()) {
					infostream << "ServerMap: Metadata loaded from "
						<< savedir << std::endl;
				} else {
					infostream << "ServerMap: Metadata could not be loaded "
						"from " << savedir << ", assuming valid save "
						"directory." << std::endl;
				}

				m_map_saving_enabled = true;
				// Map loaded, not creating new one
				// return; // KIDSCODE - Threading
			}
		}
		// If directory doesn't exist, it is safe to save to it
		else{
			m_map_saving_enabled = true;
		}
	}
	catch(std::exception &e)
	{
		warningstream<<"ServerMap: Failed to load map from "<<savedir
				<<", exception: "<<e.what()<<std::endl;
		infostream<<"Please remove the map or fix it."<<std::endl;
		warningstream<<"Map saving will be disabled."<<std::endl;
	}

	// >> KIDSCODE - Threading
	if (m_map_saving_enabled && isThreadCapable()) {
		m_map_save_thread.start();
	}
	// << KIDSCODE - Threading
}

ServerMap::~ServerMap()
{
	verbosestream<<FUNCTION_NAME<<std::endl;

	try
	{
		if (m_map_saving_enabled) {
			// >> KIDSCODE - Threading
			if (isThreadCapable()) {
				// Stop continuous saving thread
				m_map_save_thread.stop();
				m_map_save_thread.wait();
			} else {
				// Save only changed parts
				lockMultiple();
				save(MOD_STATE_WRITE_AT_UNLOAD);
				unlockMultiple();
			}
		//	// Save only changed parts
		//	save(MOD_STATE_WRITE_AT_UNLOAD);
			// << KIDSCODE - Threading
			infostream << "ServerMap: Saved map to " << m_savedir << std::endl;
		} else {
			infostream << "ServerMap: Map not saved" << std::endl;
		}
	}
	catch(std::exception &e)
	{
		infostream<<"ServerMap: Failed to save map to "<<m_savedir
				<<", exception: "<<e.what()<<std::endl;
	}

	/*
		Close database if it was opened
	*/
	delete dbase;
	if (dbase_ro)
		delete dbase_ro;

#if 0
	/*
		Free all MapChunks
	*/
	core::map<v2s16, MapChunk*>::Iterator i = m_chunks.getIterator();
	for(; i.atEnd() == false; i++)
	{
		MapChunk *chunk = i.getNode()->getValue();
		delete chunk;
	}
#endif
}

// >> KIDSCODE - Threading
s64 getBlockAsInteger(const v3s16 &pos)
{
	return (u64) pos.Z * 0x1000000 +
			(u64) pos.Y * 0x1000 +
			(u64) pos.X;
}

void ServerMap::lockBlock(const v3s16 &pos)
{
	s64 key = getBlockAsInteger(pos);
	auto holding = m_debug_locking_threads[key];
	if (holding != std::thread::id() &&
			holding != std::this_thread::get_id())

		m_block_mutexes[getBlockAsInteger(pos)].lock();
	// Debug purpose
	m_debug_locking_threads[getBlockAsInteger(pos)] = std::this_thread::get_id();
}

bool ServerMap::tryLockBlock(const v3s16 &pos)
{
	return m_block_mutexes[getBlockAsInteger(pos)].try_lock();
}

void ServerMap::unlockBlock(const v3s16 &pos)
{
	s64 key = getBlockAsInteger(pos);
	auto it = m_block_mutexes.find(key);
	if (it != m_block_mutexes.end()) {
		m_debug_locking_threads[key] = std::thread::id();
		it->second.unlock();
	}
}

/*
	Unload outdated blocks
*/

// Must be protected behind exlusive map_mutex
void ServerMap::unloadBlocks(float dtime, float unload_timeout,
		std::vector<v3s16> *unloaded_blocks)
{
	std::vector<v2s16> sector_deletion_queue;
	u32 deleted_blocks_count = 0;
	u32 block_count_all = 0;

	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;
		bool all_blocks_deleted = true;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			block->incrementUsageTimer(dtime);

			if (block->refGet() == 0
					&& block->getUsageTimer() > unload_timeout) {
				v3s16 p = block->getPos();

				// Delete only saved (blocks will be saved by save thread)
				if (block->getModified() == MOD_STATE_CLEAN) {

					// Make sure nobody else is using this block
					if (tryLockBlock(p)) {
						// Delete from memory
						sector->deleteBlock(block);

						if (unloaded_blocks)
							unloaded_blocks->push_back(p);

						deleted_blocks_count++;
						unlockBlock(p);
						continue; // Dont count this block as remaining
					}
				}
			}

			all_blocks_deleted = false;
			block_count_all++;
		}

		if (all_blocks_deleted) {
			sector_deletion_queue.push_back(sector_it.first);
		}
	}

	// Finally delete the empty sectors
	deleteSectors(sector_deletion_queue);

	if(deleted_blocks_count != 0)
	{

		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Unloaded "<<deleted_blocks_count
		<<" blocks from memory";
		infostream<<", "<<block_count_all<<" blocks in memory";
		infostream<<"."<<std::endl;
	}
}

void ServerMap::unloadUnreferencedBlocks(std::vector<v3s16> *unloaded_blocks)
{
	lockExclusive();
	save(MOD_STATE_WRITE_NEEDED);
	unloadBlocks(0.0, -1.0, unloaded_blocks);
	unlockExclusive();
}

void ServerMap::unloadOutdatedBlocks(float dtime, float unload_timeout,
		std::vector<v3s16> *unloaded_blocks)
{
	// It could be acheived with block lock
	lockExclusive();
	unloadBlocks(dtime, unload_timeout, unloaded_blocks);
	unlockExclusive();
}
// << KIDSCODE - Threading

MapgenParams *ServerMap::getMapgenParams()
{
	// getMapgenParams() should only ever be called after Server is initialized
	assert(settings_mgr.mapgen_params != NULL);
	return settings_mgr.mapgen_params;
}

u64 ServerMap::getSeed()
{
	return getMapgenParams()->seed;
}

s16 ServerMap::getWaterLevel()
{
	return getMapgenParams()->water_level;
}

bool ServerMap::blockpos_over_mapgen_limit(v3s16 p)
{
	const s16 mapgen_limit_bp = rangelim(
		getMapgenParams()->mapgen_limit, 0, MAX_MAP_GENERATION_LIMIT) /
		MAP_BLOCKSIZE;
	return p.X < -mapgen_limit_bp ||
		p.X >  mapgen_limit_bp ||
		p.Y < -mapgen_limit_bp ||
		p.Y >  mapgen_limit_bp ||
		p.Z < -mapgen_limit_bp ||
		p.Z >  mapgen_limit_bp;
}


// This function should be protected by a multiple or exclusive map mutex
bool ServerMap::initBlockMake(v3s16 blockpos, BlockMakeData *data)
{
	s16 csize = getMapgenParams()->chunksize;
	v3s16 bpmin = EmergeManager::getContainingChunk(blockpos, csize);
	v3s16 bpmax = bpmin + v3s16(1, 1, 1) * (csize - 1);

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("initBlockMake(): " PP(bpmin) " - " PP(bpmax));

	v3s16 extra_borders(1, 1, 1);
	v3s16 full_bpmin = bpmin - extra_borders;
	v3s16 full_bpmax = bpmax + extra_borders;

	// Do nothing if not inside mapgen limits (+-1 because of neighbors)
	if (blockpos_over_mapgen_limit(full_bpmin) ||
			blockpos_over_mapgen_limit(full_bpmax))
		return false;

	data->seed = getSeed();
	data->blockpos_min = bpmin;
	data->blockpos_max = bpmax;
	data->blockpos_requested = blockpos;
	data->nodedef = m_nodedef;

	/*
		Create the whole area of this and the neighboring blocks
	*/

	lockMultiple(); // KIDSCODE - Threading

	for (s16 x = full_bpmin.X; x <= full_bpmax.X; x++)
	for (s16 z = full_bpmin.Z; z <= full_bpmax.Z; z++) {
		v2s16 sectorpos(x, z);
		// Sector metadata is loaded from disk if not already loaded.
		MapSector *sector = createSector(sectorpos);
		FATAL_ERROR_IF(sector == NULL, "createSector() failed");

		for (s16 y = full_bpmin.Y; y <= full_bpmax.Y; y++) {
			v3s16 p(x, y, z);

			MapBlock *block = emergeBlock(p, false);
			if (block == NULL) {
				block = createBlock(p);

				// Block gets sunlight if this is true.
				// Refer to the map generator heuristics.
				bool ug = m_emerge->isBlockUnderground(p);
				block->setIsUnderground(ug);
			}
		}
	}

	unlockMultiple(); // KIDSCODE - Threading

	/*
		Now we have a big empty area.

		Make a ManualMapVoxelManipulator that contains this and the
		neighboring blocks
	*/

	data->vmanip = new MMVManip(this);
	data->vmanip->initialEmerge(full_bpmin, full_bpmax);

	// Data is ready now.
	return true;
}

// This function should be protected by a multiple or exclusive map mutex
void ServerMap::finishBlockMake(BlockMakeData *data,
	std::map<v3s16, MapBlock*> *changed_blocks)
{
	v3s16 bpmin = data->blockpos_min;
	v3s16 bpmax = data->blockpos_max;

	v3s16 extra_borders(1, 1, 1);

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("finishBlockMake(): " PP(bpmin) " - " PP(bpmax));

	/*
		Blit generated stuff to map
		NOTE: blitBackAll adds nearly everything to changed_blocks
	*/
	lockMultiple(); // KIDSCODE - Threading

	data->vmanip->blitBackAll(changed_blocks);

	EMERGE_DBG_OUT("finishBlockMake: changed_blocks.size()="
		<< changed_blocks->size());

	/*
		Copy transforming liquid information
	*/
	m_liquid_logic->addTransformingFromData(data);

	for (auto &changed_block : *changed_blocks) {
		MapBlock *block = changed_block.second;
		if (!block)
			continue;
		/*
			Update day/night difference cache of the MapBlocks
		*/
		block->lockBlock(); // KIDSCODE - Threading
		block->expireDayNightDiff();
		/*
			Set block as modified
		*/
		block->raiseModified(MOD_STATE_WRITE_NEEDED,
			MOD_REASON_EXPIRE_DAYNIGHTDIFF);
		block->unlockBlock(); // KIDSCODE - Threading
	}

	/*
		Set central blocks as generated
	*/
	for (s16 x = bpmin.X; x <= bpmax.X; x++)
	for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
	for (s16 y = bpmin.Y; y <= bpmax.Y; y++) {
		MapBlock *block = getBlockNoCreateNoEx(v3s16(x, y, z));
		if (!block)
			continue;

		block->lockBlock(); // KIDSCODE - Threading
		block->setGenerated(true);
		block->unlockBlock(); // KIDSCODE - Threading
	}

	unlockMultiple(); // KIDSCODE - Threading

	/*
		Save changed parts of map
		NOTE: Will be saved later.
	*/
	//save(MOD_STATE_WRITE_AT_UNLOAD);
}

MapSector *ServerMap::createSector(v2s16 p2d)
{
	/*
		Check if it exists already in memory
	*/
	MapSector *sector = getSectorNoGenerate(p2d);
	if (sector)
		return sector;

	/*
		Do not create over max mapgen limit
	*/
	const s16 max_limit_bp = MAX_MAP_GENERATION_LIMIT / MAP_BLOCKSIZE;
	if (p2d.X < -max_limit_bp ||
			p2d.X >  max_limit_bp ||
			p2d.Y < -max_limit_bp ||
			p2d.Y >  max_limit_bp)
		throw InvalidPositionException("createSector(): pos. over max mapgen limit");

	/*
		Generate blank sector
	*/

	sector = new MapSector(this, p2d, m_gamedef);

	// Sector position on map in nodes
	//v2s16 nodepos2d = p2d * MAP_BLOCKSIZE;

	/*
		Insert to container
	*/
	m_sectors[p2d] = sector;

	return sector;
}

#if 0
/*
	This is a quick-hand function for calling makeBlock().
*/
MapBlock * ServerMap::generateBlock(
		v3s16 p,
		std::map<v3s16, MapBlock*> &modified_blocks
)
{
	bool enable_mapgen_debug_info = g_settings->getBool("enable_mapgen_debug_info");

	TimeTaker timer("generateBlock");

	//MapBlock *block = original_dummy;

	v2s16 p2d(p.X, p.Z);
	v2s16 p2d_nodes = p2d * MAP_BLOCKSIZE;

	/*
		Do not generate over-limit
	*/
	if(blockpos_over_limit(p))
	{
		infostream<<FUNCTION_NAME<<": Block position over limit"<<std::endl;
		throw InvalidPositionException("generateBlock(): pos. over limit");
	}

	/*
		Create block make data
	*/
	BlockMakeData data;
	initBlockMake(&data, p);

	/*
		Generate block
	*/
	{
		TimeTaker t("mapgen::make_block()");
		mapgen->makeChunk(&data);
		//mapgen::make_block(&data);

		if(enable_mapgen_debug_info == false)
			t.stop(true); // Hide output
	}

	/*
		Blit data back on map, update lighting, add mobs and whatever this does
	*/
	finishBlockMake(&data, modified_blocks);

	/*
		Get central block
	*/
	MapBlock *block = getBlockNoCreateNoEx(p);

	if(enable_mapgen_debug_info == false)
		timer.stop(true); // Hide output

	return block;
}
#endif

MapBlock * ServerMap::createBlock(v3s16 p)
{
	/*
		Do not create over max mapgen limit
	*/
	if (blockpos_over_max_limit(p))
		throw InvalidPositionException("createBlock(): pos. over max mapgen limit");

	v2s16 p2d(p.X, p.Z);
	s16 block_y = p.Y;
	/*
		This will create or load a sector if not found in memory.
		If block exists on disk, it will be loaded.

		NOTE: On old save formats, this will be slow, as it generates
		      lighting on blocks for them.
	*/
	MapSector *sector;
	try {
		sector = createSector(p2d);
	} catch (InvalidPositionException &e) {
		infostream<<"createBlock: createSector() failed"<<std::endl;
		throw e;
	}

	/*
		Try to get a block from the sector
	*/

	MapBlock *block = sector->getBlockNoCreateNoEx(block_y);
	if (block) {
		if(block->isDummy())
			block->unDummify();
		return block;
	}
	// Create blank
	block = sector->createBlankBlock(block_y);

	return block;
}

MapBlock * ServerMap::emergeBlock(v3s16 p, bool create_blank)
{
	{
		MapBlock *block = getBlockNoCreateNoEx(p);
		if (block && !block->isDummy())
			return block;
	}

	{
		MapBlock *block = loadBlock(p);
		if(block)
			return block;
	}

	if (create_blank) {
		MapSector *sector = createSector(v2s16(p.X, p.Z));
		MapBlock *block = sector->createBlankBlock(p.Y);

		return block;
	}

	return NULL;
}

MapBlock *ServerMap::getBlockOrEmerge(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if (block == NULL)
		m_emerge->enqueueBlockEmerge(PEER_ID_INEXISTENT, p3d, false);

	return block;
}

// N.B.  This requires no synchronization, since data will not be modified unless
// the VoxelManipulator being updated belongs to the same thread.
void ServerMap::updateVManip(v3s16 pos)
{
	Mapgen *mg = m_emerge->getCurrentMapgen();
	if (!mg)
		return;

	MMVManip *vm = mg->vm;
	if (!vm)
		return;

	if (!vm->m_area.contains(pos))
		return;

	s32 idx = vm->m_area.index(pos);
	vm->m_data[idx] = getNode(pos);
	vm->m_flags[idx] &= ~VOXELFLAG_NO_DATA;

	vm->m_is_dirty = true;
}

s16 ServerMap::findGroundLevel(v2s16 p2d)
{
#if 0
	/*
		Uh, just do something random...
	*/
	// Find existing map from top to down
	s16 max=63;
	s16 min=-64;
	v3s16 p(p2d.X, max, p2d.Y);
	for(; p.Y>min; p.Y--)
	{
		MapNode n = getNodeNoEx(p);
		if(n.getContent() != CONTENT_IGNORE)
			break;
	}
	if(p.Y == min)
		goto plan_b;
	// If this node is not air, go to plan b
	if(getNodeNoEx(p).getContent() != CONTENT_AIR)
		goto plan_b;
	// Search existing walkable and return it
	for(; p.Y>min; p.Y--)
	{
		MapNode n = getNodeNoEx(p);
		if(content_walkable(n.d) && n.getContent() != CONTENT_IGNORE)
			return p.Y;
	}

	// Move to plan b
plan_b:
#endif

	/*
		Determine from map generator noise functions
	*/

	s16 level = m_emerge->getGroundLevelAtPoint(p2d);
	return level;

	//double level = base_rock_level_2d(m_seed, p2d) + AVERAGE_MUD_AMOUNT;
	//return (s16)level;
}

void ServerMap::createDirs(const std::string &path)
{
	if (!fs::CreateAllDirs(path)) {
		m_dout<<"ServerMap: Failed to create directory "
				<<"\""<<path<<"\""<<std::endl;
		throw BaseException("ServerMap failed to create directory");
	}
}

// >> KIDSCODE - Threading
// Must be protected behind exlusive or multiple map_mutex
void ServerMap::save(ModifiedState save_level, int timelimitms)
{
	std::chrono::time_point<std::chrono::system_clock> endtime;
	if (!m_map_saving_enabled) {
		warningstream<<"Not saving map, saving disabled."<<std::endl;
		return;
	}

	if(save_level == MOD_STATE_CLEAN)
		infostream<<"ServerMap: Saving whole map, this can take time."
				<<std::endl;

	if (m_map_metadata_changed || save_level == MOD_STATE_CLEAN) {
		if (settings_mgr.saveMapMeta())
			m_map_metadata_changed = false;
	}

	// Profile modified reasons
	Profiler modprofiler;

	u32 block_count = 0;

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;

	bool out_of_time = false;
	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			if(block->getModified() < (u32)save_level)
				continue;

			// Lazy beginSave()
			if(save_started) {
				if (timelimitms)
					out_of_time = std::chrono::system_clock::now() > endtime;
			} else {
				// Start transaction and compute max end time
				beginSave();
				if (timelimitms)
					endtime = std::chrono::system_clock::now() +
						std::chrono::milliseconds(timelimitms);
				save_started = true;
			}
			if(out_of_time)
				break;

			modprofiler.add(block->getModifiedReasonString(), 1);
			saveBlock(block, dbase);
			block_count++;
		}
		if(out_of_time)
			break;
	}

	if(save_started) {
		endSave();
		printf("ServerMap::save: Saved %d blocks\n", block_count);
	}

	if(block_count && out_of_time)
		// TODO: Put that in infostream
		printf("Could only save %d blocks (more to save)\n", block_count);

	/*
		Only print if something happened or saved whole map
	*/
	if(save_level == MOD_STATE_CLEAN || block_count != 0) {
		infostream<<"ServerMap: Written: "
				<<block_count<<" blocks"<<std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}

}
/*
void ServerMap::save(ModifiedState save_level)
{
	if (!m_map_saving_enabled) {
		warningstream<<"Not saving map, saving disabled."<<std::endl;
		return;
	}

	if(save_level == MOD_STATE_CLEAN)
		infostream<<"ServerMap: Saving whole map, this can take time."
				<<std::endl;

	if (m_map_metadata_changed || save_level == MOD_STATE_CLEAN) {
		if (settings_mgr.saveMapMeta())
			m_map_metadata_changed = false;
	}

	// Profile modified reasons
	Profiler modprofiler;

	u32 block_count = 0;
	u32 block_count_all = 0; // Number of blocks in memory

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;

	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			block_count_all++;

			if(block->getModified() >= (u32)save_level) {
				// Lazy beginSave()
				if(!save_started) {
					beginSave();
					save_started = true;
				}

				modprofiler.add(block->getModifiedReasonString(), 1);

				saveBlock(block);
				block_count++;
			}
		}
	}

	if(save_started)
		endSave();

	/ *
		Only print if something happened or saved whole map
	* /
	if(save_level == MOD_STATE_CLEAN
			|| block_count != 0) {
		infostream<<"ServerMap: Written: "
				<<block_count<<" block files"
				<<", "<<block_count_all<<" blocks in memory."
				<<std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}
}
*/
// << KIDSCODE - Threading

void ServerMap::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	dbase->listAllLoadableBlocks(dst);
	if (dbase_ro)
		dbase_ro->listAllLoadableBlocks(dst);
}

void ServerMap::listAllLoadedBlocks(std::vector<v3s16> &dst)
{
	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			v3s16 p = block->getPos();
			dst.push_back(p);
		}
	}
}

MapDatabase *ServerMap::createDatabase(
	const std::string &name,
	const std::string &savedir,
	Settings &conf)
{
	if (name == "sqlite3")
		return new MapDatabaseSQLite3(savedir);
	if (name == "dummy")
		return new Database_Dummy();
	#if USE_LEVELDB
	if (name == "leveldb")
		return new Database_LevelDB(savedir);
	#endif
	#if USE_REDIS
	if (name == "redis")
		return new Database_Redis(conf);
	#endif
	#if USE_POSTGRESQL
	if (name == "postgresql") {
		std::string connect_string;
		conf.getNoEx("pgsql_connection", connect_string);
		return new MapDatabasePostgreSQL(connect_string);
	}
	#endif

	throw BaseException(std::string("Database backend ") + name + " not supported.");
}

// >> KIDSCODE - Threading
bool ServerMap::isThreadCapable()
{
	return dbase->isThreadCapable();
}

void ServerMap::setThreadWriteAccess(bool writeaccess)
{
	dbase->setThreadWriteAccess(writeaccess);
}
// << KIDSCODE - Threading

void ServerMap::beginSave()
{
	dbase->beginSave();
}

void ServerMap::endSave()
{
	dbase->endSave();
}

bool ServerMap::saveBlock(MapBlock *block)
{
	// >> KIDSCODE - Threading
	lockSingle();
	bool ret = saveBlock(block, dbase);
	unlockSingle();
	return ret;
//	return saveBlock(block, dbase);
	// << KIDSCODE - Threading
}

// Cannot lock map, this version is static. It is used for map conversion at
// starting. // KIDSCODE - Threading
bool ServerMap::saveBlock(MapBlock *block, MapDatabase *db)
{
	v3s16 p3d = block->getPos();

	// Dummy blocks are not written
	if (block->isDummy()) {
		warningstream << "saveBlock: Not writing dummy block "
			<< PP(p3d) << std::endl;
		return true;
	}
	block->lockBlock(); // KIDSCODE - Threading

	// Format used for writing
	u8 version = SER_FMT_VER_HIGHEST_WRITE;

	/*
		[0] u8 serialization version
		[1] data
	*/
	std::ostringstream o(std::ios_base::binary);
	o.write((char*) &version, 1);
	block->serialize(o, version, true);
	bool ret = db->saveBlock(p3d, o.str());
	if (ret) {
		// We just wrote it to the disk so clear modified flag
		block->resetModified();
	}
	block->unlockBlock(); // KIDSCODE - Threading
	return ret;
}

void ServerMap::loadBlock(std::string *blob, v3s16 p3d, MapSector *sector, bool save_after_load)
{
	try {
		std::istringstream is(*blob, std::ios_base::binary);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char*)&version, 1);

		if(is.fail())
			throw SerializationError("ServerMap::loadBlock(): Failed"
					" to read MapBlock version");

		MapBlock *block = NULL;
		bool created_new = false;
		block = sector->getBlockNoCreateNoEx(p3d.Y);
		if(block == NULL)
		{
			block = sector->createBlankBlockNoInsert(p3d.Y);
			created_new = true;
		}

		// Read basic data
		block->deSerialize(is, version, true);

		// If it's a new block, insert it to the map
		if (created_new) {
			sector->insertBlock(block);
			m_liquid_logic->scanBlock(block);
		}
		/*
			Save blocks loaded in old format in new format
		*/

		//if(version < SER_FMT_VER_HIGHEST_READ || save_after_load)
		// Only save if asked to; no need to update version
		if(save_after_load)
			saveBlock(block);

		// We just loaded it from, so it's up-to-date.
		block->resetModified();
	}
	catch(SerializationError &e)
	{
		errorstream<<"Invalid block data in database"
				<<" ("<<p3d.X<<","<<p3d.Y<<","<<p3d.Z<<")"
				<<" (SerializationError): "<<e.what()<<std::endl;

		// TODO: Block should be marked as invalid in memory so that it is
		// not touched but the game can run

		if(g_settings->getBool("ignore_world_load_errors")){
			errorstream<<"Ignoring block load error. Duck and cover! "
					<<"(ignore_world_load_errors)"<<std::endl;
		} else {
			throw SerializationError("Invalid block data in database");
		}
	}
}

MapBlock* ServerMap::loadBlock(v3s16 blockpos)
{

	bool created_new = (getBlockNoCreateNoEx(blockpos) == NULL);

	lockSingle(); // KIDSCODE - Threading
	lockBlock(blockpos); // KIDSCODE - Threading

	v2s16 p2d(blockpos.X, blockpos.Z);

	std::string ret;
	dbase->loadBlock(blockpos, &ret);
	if (!ret.empty()) {
		loadBlock(&ret, blockpos, createSector(p2d), false);
	} else if (dbase_ro) {
		//TODO: Unknown compatibility between RO db and threaded db
		dbase_ro->loadBlock(blockpos, &ret);
		if (!ret.empty()) {
			loadBlock(&ret, blockpos, createSector(p2d), false);
		}
	} else {
		unlockBlock(blockpos); // KIDSCODE - Threading
		unlockSingle(); // KIDSCODE - Threading
		return nullptr;
	}

	unlockBlock(blockpos); // KIDSCODE - Threading
	unlockSingle(); // KIDSCODE - Threading

	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (created_new && (block != NULL)) {
		std::map<v3s16, MapBlock*> modified_blocks;
		// Fix lighting if necessary
		voxalgo::update_block_border_lighting(this, block, modified_blocks);
		if (!modified_blocks.empty()) {
			//Modified lighting, send event
			MapEditEvent event;
			event.type = MEET_OTHER;
			std::map<v3s16, MapBlock *>::iterator it;
			for (it = modified_blocks.begin();
					it != modified_blocks.end(); ++it)
				event.modified_blocks.insert(it->first);
			dispatchEvent(event);
		}
	}
	return block;
}

bool ServerMap::deleteBlock(v3s16 blockpos)
{
	lockSingle(); // KIDSCODE - Threading
	lockBlock(blockpos); // KIDSCODE - Threading

	if (!dbase->deleteBlock(blockpos)) {
		unlockBlock(blockpos); // KIDSCODE - Threading
		unlockSingle(); // KIDSCODE - Threading
		return false;
	}

	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block) {
		v2s16 p2d(blockpos.X, blockpos.Z);
		MapSector *sector = getSectorNoGenerate(p2d);
		if (!sector) {
			unlockBlock(blockpos); // KIDSCODE - Threading
			unlockSingle(); // KIDSCODE - Threading
			return false;
		}
		sector->deleteBlock(block);
	}

	unlockBlock(blockpos); // KIDSCODE - Threading
	unlockSingle(); // KIDSCODE - Threading
	return true;
}

// >> KIDSCODE - Threading
void ServerMap::transformLiquids(std::map<v3s16, MapBlock*> &modified_blocks,
		ServerEnvironment *env)
{
	if (!m_frozen)
		m_liquid_logic->transform(modified_blocks, env);
}
// << KIDSCODE - Threading

void ServerMap::PrintInfo(std::ostream &out)
{
	out<<"ServerMap: ";
}

bool ServerMap::repairBlockLight(v3s16 blockpos,
	std::map<v3s16, MapBlock *> *modified_blocks)
{
	lockSingle(); // KIDSCODE - Threading

	MapBlock *block = emergeBlock(blockpos, false);
	if (!block || !block->isGenerated()) {
		unlockSingle(); // KIDSCODE - Threading
		return false;
	}

	block->lockBlock(); // KIDSCODE - Threading
	voxalgo::repair_block_light(this, block, modified_blocks);
	block->unlockBlock(); // KIDSCODE - Threading
	unlockSingle(); // KIDSCODE - Threading

	return true;
}

// >> KIDSCODE - Threading
void ServerMap::rwCreateBackup(const std::string &backup_name, ServerEnvironment *env)
{
	lockExclusive();

	// Force loaded entities to hibernate
	env->clearActiveBlocks();
	env->deactivateFarObjects(false);
	save(MOD_STATE_WRITE_NEEDED);
	unloadBlocks(0.0, -1.0, nullptr);

	// TODO: Return if backup has been saved
	dbase->createBackup(backup_name);
	unlockExclusive();
}

/*void ServerMap::createBackup(const std::string &backup_name)
{
	// TODO: Return if backup has been saved
	dbase->createBackup(backup_name);
}
*/
// << KIDSCODE - Threading

// >> KIDSCODE - Threading
void ServerMap::rwRestoreBackup(const std::string &backup_name, ServerEnvironment *env)
{
	lockExclusive();
	m_liquid_logic->reset();
	env->clearObjects(CLEAR_OBJECTS_MODE_LOADED_ONLY);
	env->clearActiveBlocks();

/*
void ServerMap::restoreBackup(const std::string &backup_name)
{*/
// << KIDSCODE - Threading

	// Prepare a map event to tell to client that blocks have changed
	MapEditEvent event;
	event.type = MEET_OTHER;

	// Delete all map block from memory
	for (auto &sector_it : m_sectors)
	{
		MapSector *sector = sector_it.second;
		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks)
		{
			if (block->refGet() != 0) continue; // ??

			// Insert block pos into event blocks list
			event.modified_blocks.insert(block->getPos());

			// Delete from memory
			sector->deleteBlock(block);
		}

		// If sector is in sector cache, remove it from there
		if(m_sector_cache == sector)
			m_sector_cache = NULL;
		delete sector;
	}
	m_sectors.clear();

	// Restore map table to wanted savepoint state
	dbase->restoreBackup(backup_name);

	unlockExclusive(); // KIDSCODE - Threading

	// Send map event to client
	dispatchEvent(event);
}

void ServerMap::rwDeleteBackup(const std::string &backup_name, ServerEnvironment *env) // >> KIDSCODE - Threading
//void ServerMap::deleteBackup(const std::string &backup_name)
{
	env->clearActiveBlocks();  // ?? // KIDSCODE - Threading
	dbase->deleteBackup(backup_name);
}
// << KIDSCODE - Threading

void ServerMap::listBackups(std::vector<std::string> &dst)
{
	dbase->listBackups(dst);
}

// >> KIDSCODE - Threading
/*
 * Map save thread
 */

void *ServerMapSaveThread::run()
{
	if (!m_map->isThreadCapable())
		return nullptr;

	m_map->setThreadWriteAccess(true);

	while (!stopRequested()) {
		switch(m_pending_op) {
		case PO_CREATE_BACKUP:
			m_map->rwCreateBackup(m_name, m_env);
			m_pending_op = PO_NONE;
			break;
		case PO_RESTORE_BACKUP:
			m_map->rwRestoreBackup(m_name, m_env);
			m_pending_op = PO_NONE;
			break;
		case PO_DELETE_BACKUP:
			m_map->rwDeleteBackup(m_name, m_env);
			m_pending_op = PO_NONE;
			break;
		default:
			m_map->lockMultiple();
			m_map->save(MOD_STATE_WRITE_NEEDED, 100);
			m_map->unlockMultiple();
		}
		std::this_thread::yield();
	}

	// Perform a complete save before exiting
	m_map->lockMultiple();
	m_map->save(MOD_STATE_WRITE_AT_UNLOAD);
	m_map->unlockMultiple();
	return nullptr;
}

void ServerMapSaveThread::setOp(PendingOps op, const std::string &name, ServerEnvironment *env)
{
	MutexAutoLock lock(m_pending_op_mutex);
	while (m_pending_op != PO_NONE)
		std::this_thread::yield();
	m_name = name;
	m_env = env;
	m_pending_op = op;
}
// << KIDSCODE - Threading

MMVManip::MMVManip(Map *map):
		VoxelManipulator(),
		m_map(map)
{
}

void MMVManip::initialEmerge(v3s16 blockpos_min, v3s16 blockpos_max,
	bool load_if_inexistent)
{
	TimeTaker timer1("initialEmerge", &emerge_time);

	// Units of these are MapBlocks
	v3s16 p_min = blockpos_min;
	v3s16 p_max = blockpos_max;

	VoxelArea block_area_nodes
			(p_min*MAP_BLOCKSIZE, (p_max+1)*MAP_BLOCKSIZE-v3s16(1,1,1));

	u32 size_MB = block_area_nodes.getVolume()*4/1000000;
	if(size_MB >= 1)
	{
		infostream<<"initialEmerge: area: ";
		block_area_nodes.print(infostream);
		infostream<<" ("<<size_MB<<"MB)";
		infostream<<std::endl;
	}

	addArea(block_area_nodes);

	for(s32 z=p_min.Z; z<=p_max.Z; z++)
	for(s32 y=p_min.Y; y<=p_max.Y; y++)
	for(s32 x=p_min.X; x<=p_max.X; x++)
	{
		u8 flags = 0;
		MapBlock *block;
		v3s16 p(x,y,z);
		std::map<v3s16, u8>::iterator n;
		n = m_loaded_blocks.find(p);
		if(n != m_loaded_blocks.end())
			continue;

		bool block_data_inexistent = false;
		{
			TimeTaker timer2("emerge load", &emerge_load_time);

			block = m_map->getBlockNoCreateNoEx(p);
			if (!block || block->isDummy())
				block_data_inexistent = true;
			else
				block->copyTo(*this);
		}

		if(block_data_inexistent)
		{

			if (load_if_inexistent && !blockpos_over_max_limit(p)) {
				ServerMap *svrmap = (ServerMap *)m_map;
				block = svrmap->emergeBlock(p, false);
				if (block == NULL)
					block = svrmap->createBlock(p);
				block->copyTo(*this);
			} else {
				flags |= VMANIP_BLOCK_DATA_INEXIST;

				/*
					Mark area inexistent
				*/
				VoxelArea a(p*MAP_BLOCKSIZE, (p+1)*MAP_BLOCKSIZE-v3s16(1,1,1));
				// Fill with VOXELFLAG_NO_DATA
				for(s32 z=a.MinEdge.Z; z<=a.MaxEdge.Z; z++)
				for(s32 y=a.MinEdge.Y; y<=a.MaxEdge.Y; y++)
				{
					s32 i = m_area.index(a.MinEdge.X,y,z);
					memset(&m_flags[i], VOXELFLAG_NO_DATA, MAP_BLOCKSIZE);
				}
			}
		}
		/*else if (block->getNode(0, 0, 0).getContent() == CONTENT_IGNORE)
		{
			// Mark that block was loaded as blank
			flags |= VMANIP_BLOCK_CONTAINS_CIGNORE;
		}*/

		m_loaded_blocks[p] = flags;
	}

	m_is_dirty = false;
}

// This functions must be protected behind a Exclusive or Multiple map mutex lock
void MMVManip::blitBackAll(std::map<v3s16, MapBlock*> *modified_blocks,
	bool overwrite_generated)
{
	if(m_area.getExtent() == v3s16(0,0,0))
		return;

	/*
		Copy data of all blocks
	*/
	for (auto &loaded_block : m_loaded_blocks) {
		v3s16 p = loaded_block.first;
		MapBlock *block = m_map->getBlockNoCreateNoEx(p);
		bool existed = !(loaded_block.second & VMANIP_BLOCK_DATA_INEXIST);
		if (!existed || (block == NULL) ||
			(!overwrite_generated && block->isGenerated()))
			continue;

		block->lockBlock(); // KIDSCODE - Threading
		block->copyFrom(*this);
		block->raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_VMANIP);
		block->unlockBlock(); // KIDSCODE - Threading

		if(modified_blocks)
			(*modified_blocks)[p] = block;
	}
}

//END
