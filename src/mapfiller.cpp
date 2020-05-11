/*
Minetest
Copyright (C) 2020 Pierre-Yves Rollo <dev@pyrollo.com>

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
#include <time.h>
#include "mapfiller.h"
#include "mapblock.h"
#include "server.h"
#include <cstdlib>
#include <chrono>

//int dbg_time_read;

MapFiller::MapFiller(Server* srv, v3s16 min, v3s16 max):server(srv)
{
	map = &server->getMap();
	limit.min = min;
	limit.max = max;
}

int MapFiller::fill(v3s16 startpos, content_t new_cid, std::vector<content_t> old_cids,
		int yfilldir /* message */)
{
//	clock_t t = clock();
//	dbg_time_read = 0;

	// Variables set up
	nodes_count = 0;
	m_blockstofill.clear();
	m_loadedblocks.clear();

	last_request_cid = CONTENT_UNKNOWN;
	c_new = new_cid;
	c_old.clear();

	for (auto cid:old_cids)
		if (cid != c_new)
			c_old.insert(cid);

	ydir = 0;
	if (yfilldir > 0) ydir = 1;
	if (yfilldir < 0) ydir = -1;

	auto start = std::chrono::steady_clock::now();

	// Fill !
	add_block_scan(startpos);

	while (m_blockstofill.size() > 0) {
		auto it = m_blockstofill.begin();
		v3s16 pos = it->first;
		Block block = it->second;
		fill_block(&block);
		m_blockstofill.erase(pos);

		auto duration_so_far = std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::steady_clock::now() - start);
		if (duration_so_far.count() > 300) {
			server->notifyPlayers(utf8_to_wide(std::to_string(nodes_count) + " blocks remplis"));
			start = std::chrono::steady_clock::now();
		}
	}

	MapEditEvent event;
	event.type = MEET_OTHER;
	for (auto it : m_loadedblocks) {
		it.second->raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_VMANIP);
		event.modified_blocks.insert(it.first);
	}
	map->dispatchEvent(event);
	m_loadedblocks.clear();

//	t = clock() - t;
//	printf("Fill took %.2f seconds :\n", ((float)t)/CLOCKS_PER_SEC);
//	printf("  Read blocks  %.2f s\n", ((float)dbg_time_read)/CLOCKS_PER_SEC);

	return nodes_count;
}

// -- Could be replaced by mapblock getBox()
MapFiller::Bounds MapFiller::get_block_bounds(v3s16 blockpos) {
	return Bounds{
		v3s16(
			std::max(blockpos.X * MAP_BLOCKSIZE, (int)limit.min.X),
			std::max(blockpos.Y * MAP_BLOCKSIZE, (int)limit.min.Y),
			std::max(blockpos.Z * MAP_BLOCKSIZE, (int)limit.min.Z)),
		v3s16(
			std::min(blockpos.X * MAP_BLOCKSIZE + MAP_BLOCKSIZE - 1, (int)limit.max.X),
			std::min(blockpos.Y * MAP_BLOCKSIZE + MAP_BLOCKSIZE - 1, (int)limit.max.Y),
			std::min(blockpos.Z * MAP_BLOCKSIZE + MAP_BLOCKSIZE - 1, (int)limit.max.Z))
	};
}

MapFiller::Block* MapFiller::getBlock(v3s16 blockpos) {
	if (m_blockstofill.count(blockpos) == 0) {
		MapBlock *mapblock = m_loadedblocks[blockpos];
		if (mapblock == nullptr) {
//			clock_t t = clock();
			mapblock = ((ServerMap *)map)->emergeBlock(blockpos, true);
			m_loadedblocks[blockpos] = mapblock;
//			dbg_time_read += (clock() - t);
		}
		m_blockstofill[blockpos] = Block{blockpos, mapblock};
	}

	return &m_blockstofill[blockpos];
}

void MapFiller::add_block_scan(v3s16 p)
{
	if (p.X < limit.min.X || p.X > limit.max.X ||
		p.Y < limit.min.Y || p.Y > limit.max.Y ||
		p.Z < limit.min.Z || p.Z > limit.max.Z) {
		return;
	}
	getBlock(getNodeBlockPos(p))->scans.push_back(p);
}

bool MapFiller::is_to_fill(const MapNode &node)
{
	if (node.getContent() != last_request_cid) {
		last_request_cid = node.getContent();
		last_request_response = c_old.count(node.getContent()) > 0;
	}
	return last_request_response;
}

void MapFiller::fill_block(Block *block)
{
	Bounds bounds = get_block_bounds(block->pos);
	MapNode *data = block->block->getData();
	int index;

	while (block->scans.size() > 0) {
		v3s16 pos = block->scans.back();
		block->scans.pop_back();

		// Find the west end of the line
		index = block->Index(pos);

		s16 startx = pos.X;
		while (startx >= bounds.min.X && is_to_fill(data[index])) {
			index--;
			startx--;
		}
		startx++;

		// Find the east end of the line
		index = block->Index(pos);
		s16 endx = pos.X;
		while (endx <= bounds.max.X && is_to_fill(data[index])) {
			endx++;
			index++;
		}
		endx--;

		// Already filled
		if (startx > endx)
			continue;

		// Add position to neighboor block if overflows
		if (startx == bounds.min.X && startx > limit.min.X)
			add_block_scan(v3s16(startx - 1, pos.Y, pos.Z));

		// Add position to neighboor block if overflows
		if (endx == bounds.max.X && endx < limit.max.X)
			add_block_scan(v3s16(endx + 1, pos.Y, pos.Z));

		// Actually fill line
		index = block->Index(startx, pos.Y, pos.Z);
		for (s16 x = startx; x <= endx; x++) {
			data[index].setContent(c_new);
			index++;
			nodes_count++;
		}

		// Probe neighboors in two or three directions (+z, -z and +or-y)
		for (s16 v = -1; v <= 1; v++) {
			if (v == 0 && ydir == 0)
				continue; // No yfill (2 directions probing only)

			s16 y = pos.Y + ((v == 0)?ydir:0); // 0 Vertical probing
			s16 z = pos.Z + v; // -1, 1 horizontal probing

			if (y < limit.min.Y || y > limit.max.Y ||
				z < limit.min.Z || z > limit.max.Z)
				continue;

			Block *probeblock;
			MapNode *probedata;

			if (y < bounds.min.Y || y > bounds.max.Y ||
				z < bounds.min.Z || z > bounds.max.Z) {
				// Neighboor in another block
				probeblock = getBlock(getNodeBlockPos(v3s16(startx, y, z)));
				probedata = probeblock->block->getData();
			} else {
				probeblock = block;
				probedata = data;
			}

			int probeindex = probeblock->Index(startx, y, z);

			s16 probex;
			bool started = false;
			for (s16 x = startx; x <= endx; x++) {
				if (started) {
					if (!is_to_fill(probedata[probeindex])) {
						// End of fillable neighbor line --> Stack position
						if (probeblock == block)
							block->scans.push_back(v3s16(probex, y, z));
						else
							add_block_scan(v3s16(probex, y, z));
						started = false;
					}
				} else {
					if (is_to_fill(probedata[probeindex])) {
						// Start of fillable neighbor line
						probex = x;
						started = true;
					}
				}
				probeindex++;
			}

			// Unfinished neighbor line
			if (started) {
				if (probeblock == block)
					block->scans.push_back(v3s16(probex, y, z));
				else
					add_block_scan(v3s16(probex, y, z));
			}
		}
	} // while (block->scans.size() > 0)
} // fill_block
