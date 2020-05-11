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

#pragma once

#include <vector>
#include "irrlichttypes_bloated.h"
#include "map.h"

class Server;
class MMxManip;

class MapFiller
{
public:
	MapFiller(Server* srv, v3s16 min, v3s16 max);
	int fill(v3s16 startpos, content_t new_cid, std::vector<content_t> old_cids,
			int yfilldir);

protected:
	struct Bounds {
		v3s16 min, max;
	};

	struct Block {
		v3s16 pos;
		MapBlock *block;
		std::vector<v3s16> scans;

		int Index(s16 x, s16 y, s16 z) {
			return (x - pos.X * MAP_BLOCKSIZE) +
					(y - pos.Y * MAP_BLOCKSIZE) * MAP_BLOCKSIZE +
					(z - pos.Z * MAP_BLOCKSIZE) * MAP_BLOCKSIZE * MAP_BLOCKSIZE;
		};

		int Index(v3s16 p) { return Index(p.X, p.Y, p.Z); };

	};

	Block* getBlock(v3s16 blockpos);
	Bounds get_block_bounds(v3s16 blockpos);
	void add_block_scan(v3s16 p);
	bool is_to_fill(const MapNode &cid);
	void fill_block(Block *block);

	// Persistant class members
	Map *map;
	Server *server;
	Bounds limit;

	// During fill class members
	int ydir;
	int nodes_count;
	std::set<content_t> c_old;
	content_t c_new;
	content_t last_request_cid;
	bool last_request_response;
	std::map<v3s16, Block> m_blockstofill;
	std::map<v3s16, MapBlock*> m_loadedblocks;
};
