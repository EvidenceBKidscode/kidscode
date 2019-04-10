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

class LiquidLogicPreserve: public LiquidLogic {
public:
	LiquidLogicPreserve(Map *map, IGameDef *gamedef);
	void addTransforming(v3s16 p);
	void scanBlock(MapBlock *block);
	void scanVoxelManip(MMVManip *vm, v3s16 nmin, v3s16 nmax);
	void transform(std::map<v3s16, MapBlock*> &modified_blocks,
		ServerEnvironment *env);
	void addTransformingFromData(BlockMakeData *data);

private:
	void setNodeLevel(
		MapNode &n, s8 l, bool flowing_down,
		content_t c_source, content_t c_flowing, content_t c_empty);
	s8 getNodeLevel(MapNode &n, content_t c_source);
	void updateNodeIfChanged(v3s16 pos, MapNode nnew, MapNode nold,
		std::map<v3s16, MapBlock*> &modified_blocks, ServerEnvironment *env);

	UniqueQueue<v3s16> m_liquid_queue;
	std::deque<v3s16> m_must_reflow;
	std::vector<std::pair<v3s16, MapNode> > m_changed_nodes;
	v3s16 m_block_pos, m_rel_block_pos;
	u32 m_unprocessed_count = 0;
	bool m_queue_size_timer_started = false;
	u64 m_inc_trending_up_start_time = 0; // milliseconds
};
