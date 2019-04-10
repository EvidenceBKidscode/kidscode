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

#include "liquidlogic.h"
#include "map.h"
#include "nodedef.h"

LiquidLogic::LiquidLogic(Map *map, IGameDef *gamedef) :
	m_map(map),
	m_gamedef(gamedef)
{
	m_ndef = m_map->getNodeDefManager();
}

void LiquidLogic::addTransforming(v3s16 p)
{ }

void LiquidLogic::scanBlock(MapBlock *block)
{ }

void LiquidLogic::scanVoxelManip(MMVManip *vm, v3s16 nmin, v3s16 nmax)
{ }

void LiquidLogic::transform(std::map<v3s16, MapBlock*> &modified_blocks,
	ServerEnvironment *env)
{
	printf("LiquidLogic transform\n");
}

void LiquidLogic::addTransformingFromData(BlockMakeData *data)
{ }
