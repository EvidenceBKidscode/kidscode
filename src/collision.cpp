/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "collision.h"
#include <cmath>
#include "mapblock.h"
#include "map.h"
#include "nodedef.h"
#include "gamedef.h"
#ifndef SERVER
#include "client/clientenvironment.h"
#endif
#include "serverenvironment.h"
#include "serverobject.h"
#include "util/timetaker.h"
#include "profiler.h"

// float error is 10 - 9.96875 = 0.03125
//#define COLL_ZERO 0.032 // broken unit tests
#define COLL_ZERO 0

struct NearbyCollisionInfo
{
	NearbyCollisionInfo(bool is_ul, bool is_obj, int bouncy,
		const v3s16 &pos, const aabb3f &box) :
		is_unloaded(is_ul),
		is_object(is_obj),
		bouncy(bouncy),
		position(pos),
		box(box)
	{}

	bool is_unloaded;
	bool is_step_up = false;
	bool is_object;
	int bouncy;
	v3s16 position;
	aabb3f box;
};


// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// The time after which the collision occurs is stored in dtime.
CollisionAxis axisAlignedCollision(
		const aabb3f &staticbox, const aabb3f &movingbox,
		const v3f &speed, f32 d, f32 *dtime)
{
	//TimeTaker tt("axisAlignedCollision");

	// reduce box size for solve collision stuck (flying sand)
	f32 xsize = (staticbox.MaxEdge.X - staticbox.MinEdge.X) - COLL_ZERO;
	// Y - no sense for falling, but maybe try later
	f32 ysize = (staticbox.MaxEdge.Y - staticbox.MinEdge.Y); // - COLL_ZERO;
	f32 zsize = (staticbox.MaxEdge.Z - staticbox.MinEdge.Z) - COLL_ZERO;

	aabb3f relbox(
		movingbox.MinEdge.X - staticbox.MinEdge.X,
		movingbox.MinEdge.Y - staticbox.MinEdge.Y,
		movingbox.MinEdge.Z - staticbox.MinEdge.Z,
		movingbox.MaxEdge.X - staticbox.MinEdge.X,
		movingbox.MaxEdge.Y - staticbox.MinEdge.Y,
		movingbox.MaxEdge.Z - staticbox.MinEdge.Z
	);

	if (speed.X > 0) { // Check for collision with X- plane
		if (relbox.MaxEdge.X <= d) {
			*dtime = -relbox.MaxEdge.X / speed.X;
			if ((relbox.MinEdge.Y + speed.Y * (*dtime) < ysize)                 &&
					(relbox.MaxEdge.Y + speed.Y * (*dtime) > COLL_ZERO) &&
					(relbox.MinEdge.Z + speed.Z * (*dtime) < zsize)     &&
					(relbox.MaxEdge.Z + speed.Z * (*dtime) > COLL_ZERO))
				return COLLISION_AXIS_X;
		}
		else if(relbox.MinEdge.X > xsize)
		{
			return COLLISION_AXIS_NONE;
		}
	} else if (speed.X < 0) { // Check for collision with X+ plane
		if (relbox.MinEdge.X >= xsize - d) {
			*dtime = (xsize - relbox.MinEdge.X) / speed.X;
			if ((relbox.MinEdge.Y + speed.Y * (*dtime) < ysize)                 &&
					(relbox.MaxEdge.Y + speed.Y * (*dtime) > COLL_ZERO) &&
					(relbox.MinEdge.Z + speed.Z * (*dtime) < zsize)     &&
					(relbox.MaxEdge.Z + speed.Z * (*dtime) > COLL_ZERO))
				return COLLISION_AXIS_X;
		}
		else if(relbox.MaxEdge.X < 0)
		{
			return COLLISION_AXIS_NONE;
		}
	}
	// NO else if here

	if (speed.Y > 0) { // Check for collision with Y- plane
		if (relbox.MaxEdge.Y <= d) {
			*dtime = -relbox.MaxEdge.Y / speed.Y;
			if ((relbox.MinEdge.X + speed.X * (*dtime) < xsize)                 &&
					(relbox.MaxEdge.X + speed.X * (*dtime) > COLL_ZERO) &&
					(relbox.MinEdge.Z + speed.Z * (*dtime) < zsize)     &&
					(relbox.MaxEdge.Z + speed.Z * (*dtime) > COLL_ZERO))
				return COLLISION_AXIS_Y;
		}
		else if(relbox.MinEdge.Y > ysize)
		{
			return COLLISION_AXIS_NONE;
		}
	} else if (speed.Y < 0) { // Check for collision with Y+ plane
		if (relbox.MinEdge.Y >= ysize - d) {
			*dtime = (ysize - relbox.MinEdge.Y) / speed.Y;
			if ((relbox.MinEdge.X + speed.X * (*dtime) < xsize)                 &&
					(relbox.MaxEdge.X + speed.X * (*dtime) > COLL_ZERO) &&
					(relbox.MinEdge.Z + speed.Z * (*dtime) < zsize)     &&
					(relbox.MaxEdge.Z + speed.Z * (*dtime) > COLL_ZERO))
				return COLLISION_AXIS_Y;
		}
		else if(relbox.MaxEdge.Y < 0)
		{
			return COLLISION_AXIS_NONE;
		}
	}
	// NO else if here

	if (speed.Z > 0) { // Check for collision with Z- plane
		if (relbox.MaxEdge.Z <= d) {
			*dtime = -relbox.MaxEdge.Z / speed.Z;
			if ((relbox.MinEdge.X + speed.X * (*dtime) < xsize)                 &&
					(relbox.MaxEdge.X + speed.X * (*dtime) > COLL_ZERO) &&
					(relbox.MinEdge.Y + speed.Y * (*dtime) < ysize)     &&
					(relbox.MaxEdge.Y + speed.Y * (*dtime) > COLL_ZERO))
				return COLLISION_AXIS_Z;
		}
		//else if (relbox.MinEdge.Z > zsize)
		//{
		//	return COLLISION_AXIS_NONE;
		//}
	} else if (speed.Z < 0) { // Check for collision with Z+ plane
		if (relbox.MinEdge.Z >= zsize - d) {
			*dtime = (zsize - relbox.MinEdge.Z) / speed.Z;
			if ((relbox.MinEdge.X + speed.X * (*dtime) < xsize)                 &&
					(relbox.MaxEdge.X + speed.X * (*dtime) > COLL_ZERO) &&
					(relbox.MinEdge.Y + speed.Y * (*dtime) < ysize)     &&
					(relbox.MaxEdge.Y + speed.Y * (*dtime) > COLL_ZERO))
				return COLLISION_AXIS_Z;
		}
		//else if (relbox.MaxEdge.Z < 0)
		//{
		//	return COLLISION_AXIS_NONE;
		//}
	}

	return COLLISION_AXIS_NONE;
}

std::tuple<int, f32, bool> pointBoxCollision(aabb3f staticbox, const v3f &pos,
	const v3f &speed, f32 surface_dist)
{
	f32 dtime = 0.f;

	// Keep distance from collision box
	staticbox.MinEdge -= surface_dist;
	staticbox.MaxEdge += surface_dist;
	v3f relpos = pos - staticbox.MinEdge;

	f32 xsize = (staticbox.MaxEdge.X - staticbox.MinEdge.X);
	f32 ysize = (staticbox.MaxEdge.Y - staticbox.MinEdge.Y);
	f32 zsize = (staticbox.MaxEdge.Z - staticbox.MinEdge.Z);

	bool inside = relpos.X > 0 && relpos.X < xsize &&
		relpos.Y > 0 && relpos.Y < ysize &&
		relpos.Z > 0 && relpos.Z < zsize;

	// Possible to move backwards in time if inside collision box
	v3f d;
	if (inside)
		d = {0.5f * xsize, 0.5f * ysize, 0.5f * zsize};
	else
		d = {0.f, 0.f, 0.f};

	if (speed.X > 0) { // Check for collision with X- plane
		if (relpos.X <= d.X) {
			dtime = -relpos.X / speed.X;
			if ((relpos.Y + speed.Y * (dtime) < ysize)             &&
					(relpos.Y + speed.Y * (dtime) > 0)     &&
					(relpos.Z + speed.Z * (dtime) < zsize) &&
					(relpos.Z + speed.Z * (dtime) > 0)) {
				return std::make_tuple(0, dtime, inside);
			}
		} else if (relpos.X > xsize) {
			return std::make_tuple(-1, dtime, inside);
		}
	} else if (speed.X < 0) { // Check for collision with X+ plane
		if (relpos.X >= xsize - d.X) {
			dtime = (xsize - relpos.X) / speed.X;
			if ((relpos.Y + speed.Y * (dtime) < ysize)             &&
					(relpos.Y + speed.Y * (dtime) > 0)     &&
					(relpos.Z + speed.Z * (dtime) < zsize) &&
					(relpos.Z + speed.Z * (dtime) > 0))
				return std::make_tuple(0, dtime, inside);
		} else if (relpos.X < 0) {
			return std::make_tuple(-1, dtime, inside);
		}
	}
	// NO else if here

	if (speed.Y > 0) { // Check for collision with Y- plane
		if (relpos.Y <= d.Y) {
			dtime = -relpos.Y / speed.Y;
			if ((relpos.X + speed.X * (dtime) < xsize)             &&
					(relpos.X + speed.X * (dtime) > 0)     &&
					(relpos.Z + speed.Z * (dtime) < zsize) &&
					(relpos.Z + speed.Z * (dtime) > 0))
				return std::make_tuple(1, dtime, inside);
		} else if (relpos.Y > ysize) {
			return std::make_tuple(-1, dtime, inside);
		}
	} else if (speed.Y < 0) { // Check for collision with Y+ plane
		if (relpos.Y >= ysize - d.Y) {
			dtime = (ysize - relpos.Y) / speed.Y;
			if ((relpos.X + speed.X * (dtime) < xsize)             &&
					(relpos.X + speed.X * (dtime) > 0)     &&
					(relpos.Z + speed.Z * (dtime) < zsize) &&
					(relpos.Z + speed.Z * (dtime) > 0))
				return std::make_tuple(1, dtime, inside);
		} else if (relpos.Y < 0) {
			return std::make_tuple(-1, dtime, inside);
		}
	}
	// NO else if here

	if (speed.Z > 0) { // Check for collision with Z- plane
		if (relpos.Z <= d.Z) {
			dtime = -relpos.Z / speed.Z;
			if ((relpos.X + speed.X * (dtime) < xsize)             &&
					(relpos.X + speed.X * (dtime) > 0)     &&
					(relpos.Y + speed.Y * (dtime) < ysize) &&
					(relpos.Y + speed.Y * (dtime) > 0))
				return std::make_tuple(2, dtime, inside);
		} else if (relpos.Z > zsize) {
			return std::make_tuple(-1, dtime, inside);
		}
	} else if (speed.Z < 0) { // Check for collision with Z+ plane
		if (relpos.Z >= zsize - d.Z) {
			dtime = (zsize - relpos.Z) / speed.Z;
			if ((relpos.X + speed.X * (dtime) < xsize)             &&
					(relpos.X + speed.X * (dtime) > 0)     &&
					(relpos.Y + speed.Y * (dtime) < ysize) &&
					(relpos.Y + speed.Y * (dtime) > 0))
				return std::make_tuple(2, dtime, inside);
		} else if (relpos.Z < 0) {
			return std::make_tuple(-1, dtime, inside);
		}
	}

	return std::make_tuple(-1, dtime, inside);
}

// Helper function:
// Checks if moving the movingbox up by the given distance would hit a ceiling.
bool wouldCollideWithCeiling(
	const std::vector<NearbyCollisionInfo> &cinfo,
	const aabb3f &movingbox,
	f32 y_increase, f32 d)
{
	//TimeTaker tt("wouldCollideWithCeiling");

	assert(y_increase >= 0);	// pre-condition

	for (const auto &it : cinfo) {
		const aabb3f &staticbox = it.box;
		if ((movingbox.MaxEdge.Y - d <= staticbox.MinEdge.Y)                     &&
				(movingbox.MaxEdge.Y + y_increase > staticbox.MinEdge.Y) &&
				(movingbox.MinEdge.X < staticbox.MaxEdge.X)              &&
				(movingbox.MaxEdge.X > staticbox.MinEdge.X)              &&
				(movingbox.MinEdge.Z < staticbox.MaxEdge.Z)              &&
				(movingbox.MaxEdge.Z > staticbox.MinEdge.Z))
			return true;
	}

	return false;
}

static inline void getNeighborConnectingFace(const v3s16 &p,
	const NodeDefManager *nodedef, Map *map, MapNode n, int v, int *neighbors)
{
	MapNode n2 = map->getNode(p);
	if (nodedef->nodeboxConnects(n, n2, v))
		*neighbors |= v;
}

static void collect_object_nodeboxes(std::vector<NearbyCollisionInfo> &cinfo,
	Environment *env, f32 dtime, v3f *pos_f, v3f *speed_f, ActiveObject *self,
	f32 box_length)
{

	// add object boxes to cinfo
	std::vector<ActiveObject*> objects;
#ifndef SERVER
	ClientEnvironment *c_env = dynamic_cast<ClientEnvironment*>(env);
	if (c_env != 0) {
		// Calculate distance by speed, add own extent and 1.5m of tolerance
		f32 distance = speed_f->getLength() * dtime +
			box_length + 1.5f * BS;
		std::vector<DistanceSortedActiveObject> clientobjects;
		c_env->getActiveObjects(*pos_f, distance, clientobjects);

		for (auto &clientobject : clientobjects) {
			// Do collide with everything but itself and the parent CAO
			if (!self || (self != clientobject.obj &&
					self != clientobject.obj->getParent())) {
				objects.push_back((ActiveObject*) clientobject.obj);
			}
		}
	} else
#endif
	{
		ServerEnvironment *s_env = dynamic_cast<ServerEnvironment*>(env);
		if (s_env != NULL) {
			// Calculate distance by speed, add own extent and 1.5m of tolerance
			f32 distance = speed_f->getLength() * dtime +
				box_length + 1.5f * BS;
			std::vector<u16> s_objects;
			s_env->getObjectsInsideRadius(s_objects, *pos_f, distance);

			for (u16 obj_id : s_objects) {
				ServerActiveObject *current = s_env->getActiveObject(obj_id);

				if (!self || (self != current &&
						self != current->getParent())) {
					objects.push_back((ActiveObject*)current);
				}
			}
		}
	}

	for (std::vector<ActiveObject*>::const_iterator iter = objects.begin();
			iter != objects.end(); ++iter) {
		ActiveObject *object = *iter;

		if (object) {
			aabb3f object_collisionbox;
			if (object->getCollisionBox(&object_collisionbox) &&
					object->collideWithObjects()) {
				cinfo.emplace_back(false, true, 0,
					v3s16(), object_collisionbox);
			}
		}
	}
}

static bool collect_node_nodeboxes(std::vector<NearbyCollisionInfo> &cinfo,
	Map *map, v3s16 p, IGameDef *gamedef)
{
	bool is_position_valid;
	MapNode n = map->getNode(p, &is_position_valid);

	if (is_position_valid && n.getContent() != CONTENT_IGNORE) {
		// Object collides into walkable nodes

		const NodeDefManager *nodedef = gamedef->getNodeDefManager();
		const ContentFeatures &f = nodedef->get(n);

		if (f.walkable) {
			int neighbors = 0;
			if (f.drawtype == NDT_NODEBOX &&
				f.node_box.type == NODEBOX_CONNECTED) {
				v3s16 p2 = p;

				p2.Y++;
				getNeighborConnectingFace(p2, nodedef,
					map, n, 1, &neighbors);

				p2 = p;
				p2.Y--;
				getNeighborConnectingFace(p2, nodedef,
					map, n, 2, &neighbors);

				p2 = p;
				p2.Z--;
				getNeighborConnectingFace(p2, nodedef,
					map, n, 4, &neighbors);

				p2 = p;
				p2.X--;
				getNeighborConnectingFace(p2, nodedef,
					map, n, 8, &neighbors);

				p2 = p;
				p2.Z++;
				getNeighborConnectingFace(p2, nodedef,
					map, n, 16, &neighbors);

				p2 = p;
				p2.X++;
				getNeighborConnectingFace(p2, nodedef,
					map, n, 32, &neighbors);
			}
			std::vector<aabb3f> nodeboxes;
			n.getCollisionBoxes(gamedef->ndef(), &nodeboxes, neighbors);

			// Calculate float position only once
			v3f posf = intToFloat(p, BS);
			for (auto box : nodeboxes) {
				box.MinEdge += posf;
				box.MaxEdge += posf;
				cinfo.emplace_back(false, false, 0, p, box);
			}
		}
		return true;
	} else {
		/*
		 * Collide with unloaded nodes (position invalid) and loaded
		 * CONTENT_IGNORE nodes (position valid)
		 */
		aabb3f box = getNodeBox(p, BS);
		cinfo.emplace_back(true, false, 0, p, box);
		return false;
	}
}

collisionMoveResult collisionMoveSimple(Environment *env, IGameDef *gamedef,
	f32 pos_max_d, const aabb3f &box_0,
	f32 stepheight, f32 dtime,
	v3f *pos_f, v3f *speed_f,
	v3f accel_f, ActiveObject *self,
	bool collideWithObjects)
{
	static bool time_notification_done = false;
	Map *map = &env->getMap();

	ScopeProfiler sp(g_profiler, "collisionMoveSimple()", SPT_AVG);

	collisionMoveResult result;

	// Calculate new velocity
	if (dtime > 0.5f) {
		if (!time_notification_done) {
			time_notification_done = true;
			infostream << "collisionMoveSimple: maximum step interval"
				" exceeded, lost movement details!" << std::endl;
		}
		dtime = 0.5f;
	} else {
		time_notification_done = false;
	}
	*speed_f += accel_f * dtime;

	// If there is no speed, there are no collisions
	if (speed_f->getLength() == 0)
		return result;

	// Limit speed for avoiding hangs
	speed_f->Y = rangelim(speed_f->Y, -5000, 5000);
	speed_f->X = rangelim(speed_f->X, -5000, 5000);
	speed_f->Z = rangelim(speed_f->Z, -5000, 5000);

	// Collect node boxes in movement range
	std::vector<NearbyCollisionInfo> cinfo;
	{

	//TimeTaker tt2("collisionMoveSimple collect boxes");
	ScopeProfiler sp2(g_profiler, "collisionMoveSimple(): collect boxes", SPT_AVG);

	v3f newpos_f = *pos_f + *speed_f * dtime;
	v3f minpos_f(
		MYMIN(pos_f->X, newpos_f.X),
		// bias rounding, player often at +/-n.5
		MYMIN(pos_f->Y, newpos_f.Y) + 0.01f * BS,
		MYMIN(pos_f->Z, newpos_f.Z)
	);
	v3f maxpos_f(
		MYMAX(pos_f->X, newpos_f.X),
		MYMAX(pos_f->Y, newpos_f.Y),
		MYMAX(pos_f->Z, newpos_f.Z)
	);
	v3s16 min = floatToInt(minpos_f + box_0.MinEdge, BS) - v3s16(1, 1, 1);
	v3s16 max = floatToInt(maxpos_f + box_0.MaxEdge, BS) + v3s16(1, 1, 1);

	bool any_position_valid = false;

	v3s16 p;
	for (p.X = min.X; p.X <= max.X; p.X++)
	for (p.Y = min.Y; p.Y <= max.Y; p.Y++)
	for (p.Z = min.Z; p.Z <= max.Z; p.Z++) {
		any_position_valid |= collect_node_nodeboxes(cinfo, map, p, gamedef);
	}

	// Do not move if world has not loaded yet, since custom node boxes
	// are not available for collision detection.
	// This also intentionally occurs in the case of the object being positioned
	// solely on loaded CONTENT_IGNORE nodes, no matter where they come from.
	if (!any_position_valid) {
		*speed_f = v3f(0, 0, 0);
		return result;
	}

	} // tt2

	if (collideWithObjects) {
		collect_object_nodeboxes(cinfo, env, dtime, pos_f, speed_f, self, box_0.getExtent().getLength());
		ScopeProfiler sp2(g_profiler, "collisionMoveSimple objects avg", SPT_AVG);
		// TimeTaker tt3("collisionMoveSimple collect object boxes");
	} //tt3

	// Collision detection
	/*
		Collision detection
	*/

	/*
		Collision uncertainty radius
		Make it a bit larger than the maximum distance of movement
	*/
	f32 d = pos_max_d * 1.1f;
	// A fairly large value in here makes moving smoother
	//f32 d = 0.15*BS;

	// This should always apply, otherwise there are glitches
	assert(d > pos_max_d);	// invariant

	int loopcount = 0;

	while (dtime > BS * 1e-10f) {
		// TimeTaker tt3("collisionMoveSimple dtime loop");
		ScopeProfiler sp2(g_profiler, "collisionMoveSimple dtime loop avg", SPT_AVG);

		// Avoid infinite loop
		loopcount++;
		if (loopcount >= 100) {
			warningstream << "collisionMoveSimple: Loop count exceeded,"
				" aborting to avoid infiniite loop" << std::endl;
			break;
		}

		aabb3f movingbox = box_0;
		movingbox.MinEdge += *pos_f;
		movingbox.MaxEdge += *pos_f;

		CollisionAxis nearest_collided = COLLISION_AXIS_NONE;
		f32 nearest_dtime = dtime;
		int nearest_boxindex = -1;

		// Go through every nodebox, find nearest collision
		for (u32 boxindex = 0; boxindex < cinfo.size(); boxindex++) {
			const NearbyCollisionInfo &box_info = cinfo[boxindex];
			// Ignore if already stepped up this nodebox.
			if (box_info.is_step_up)
				continue;

			// Find nearest collision of the two boxes (raytracing-like)
			f32 dtime_tmp;
			CollisionAxis collided = axisAlignedCollision(box_info.box,
					movingbox, *speed_f, d, &dtime_tmp);

			if (collided == -1 || dtime_tmp >= nearest_dtime)
				continue;

			nearest_dtime = dtime_tmp;
			nearest_collided = collided;
			nearest_boxindex = boxindex;
		}

		if (nearest_collided == COLLISION_AXIS_NONE) {
			// No collision with any collision box.
			*pos_f += *speed_f * dtime;
			dtime = 0; // Set to 0 to avoid "infinite" loop due to small FP numbers
		} else {
			// Otherwise, a collision occurred.
			NearbyCollisionInfo &nearest_info = cinfo[nearest_boxindex];
			const aabb3f& cbox = nearest_info.box;
			// Check for stairs.

			bool step_up = (nearest_collided != COLLISION_AXIS_Y) && // must not be Y direction
					(movingbox.MinEdge.Y < cbox.MaxEdge.Y) &&
					(movingbox.MinEdge.Y + stepheight > cbox.MaxEdge.Y) &&
					(!wouldCollideWithCeiling(cinfo, movingbox,
							cbox.MaxEdge.Y - movingbox.MinEdge.Y,
							d));

			// Get bounce multiplier
			float bounce = -(float) nearest_info.bouncy / 100.0f;

			// Move to the point of collision and reduce dtime by nearest_dtime
			if (nearest_dtime < 0) {
				// Handle negative nearest_dtime (can be caused by the d allowance)
				if (!step_up) {
					if (nearest_collided == COLLISION_AXIS_X)
						pos_f->X += speed_f->X * nearest_dtime;
					if (nearest_collided == COLLISION_AXIS_Y)
						pos_f->Y += speed_f->Y * nearest_dtime;
					if (nearest_collided == COLLISION_AXIS_Z)
						pos_f->Z += speed_f->Z * nearest_dtime;
				}
			} else {
				*pos_f += *speed_f * nearest_dtime;
				dtime -= nearest_dtime;
			}

			bool is_collision = true;
			if (nearest_info.is_unloaded)
				is_collision = false;

			CollisionInfo info;
			if (nearest_info.is_object)
				info.type = COLLISION_OBJECT;
			else
				info.type = COLLISION_NODE;

			info.node_p = nearest_info.position;
			info.old_speed = *speed_f;
			info.plane = nearest_collided;

			// Set the speed component that caused the collision to zero
			if (step_up) {
				// Special case: Handle stairs
				nearest_info.is_step_up = true;
				is_collision = false;
			} else if (nearest_collided == COLLISION_AXIS_X) {
				if (fabs(speed_f->X) > BS * 3)
					speed_f->X *= bounce;
				else
					speed_f->X = 0;
				result.collides = true;
			} else if (nearest_collided == COLLISION_AXIS_Y) {
				if(fabs(speed_f->Y) > BS * 3)
					speed_f->Y *= bounce;
				else
					speed_f->Y = 0;
				result.collides = true;
			} else if (nearest_collided == COLLISION_AXIS_Z) {
				if (fabs(speed_f->Z) > BS * 3)
					speed_f->Z *= bounce;
				else
					speed_f->Z = 0;
				result.collides = true;
			}

			info.new_speed = *speed_f;
			if (info.new_speed.getDistanceFrom(info.old_speed) < 0.1f * BS)
				is_collision = false;

			if (is_collision) {
				info.axis = nearest_collided;
				result.collisions.push_back(info);
			}
		}
	}

	// Final touches: Check if standing on ground, step up stairs.
	aabb3f box = box_0;
	box.MinEdge += *pos_f;
	box.MaxEdge += *pos_f;
	for (const auto &box_info : cinfo) {
		const aabb3f &cbox = box_info.box;

		/*
		 * See if the object is touching ground.
		 *
		 * Object touches ground if object's minimum Y is near node's
		 * maximum Y and object's X-Z-area overlaps with the node's
		 * X-Z-area.
		 *
		 * Use 0.15*BS so that it is easier to get on a node.
		 */
		if (cbox.MaxEdge.X - d > box.MinEdge.X && cbox.MinEdge.X + d < box.MaxEdge.X &&
				cbox.MaxEdge.Z - d > box.MinEdge.Z &&
				cbox.MinEdge.Z + d < box.MaxEdge.Z) {
			if (box_info.is_step_up) {
				pos_f->Y += cbox.MaxEdge.Y - box.MinEdge.Y;
				box = box_0;
				box.MinEdge += *pos_f;
				box.MaxEdge += *pos_f;
			}
			if (std::fabs(cbox.MaxEdge.Y - box.MinEdge.Y) < 0.15f * BS) {
				result.touching_ground = true;

				if (box_info.is_object)
					result.standing_on_object = true;
			}
		}
	}

	return result;
}


collisionMoveResult collisionMovePoint(Environment *env, IGameDef *gamedef,
	f32 surface_dist, f32 dtime, v3f *pos_f, v3f *speed_f,
	v3f accel_f, ActiveObject *self, f32 bounce_fraction,
	f32 bounce_threshold, bool collideWithObjects)
{
	static bool time_notification_done = false;
	Map *map = &env->getMap();
	ScopeProfiler sp(g_profiler, "collisionMovePoint avg", SPT_AVG);

	collisionMoveResult result;

	// Calculate new velocity
	if (dtime > 0.5f) {
		if (!time_notification_done) {
			time_notification_done = true;
			infostream << "collisionMoveSimple: maximum step interval"
				" exceeded, lost movement details!" << std::endl;
		}
		dtime = 0.5f;
	} else {
		time_notification_done = false;
	}
	*speed_f += accel_f * dtime;

	// If there is no speed, there are no collisions
	if (speed_f->getLength() == 0)
		return result;

	// Limit speed for avoiding hangs
	speed_f->Y = rangelim(speed_f->Y, -50*BS, 50*BS);
	speed_f->X = rangelim(speed_f->X, -50*BS, 50*BS);
	speed_f->Z = rangelim(speed_f->Z, -50*BS, 50*BS);

	std::vector<NearbyCollisionInfo> cinfo_objects;
	if (collideWithObjects) {
		ScopeProfiler sp2(g_profiler, "collisionMovePoint objects avg", SPT_AVG);
		collect_object_nodeboxes(cinfo_objects, env, dtime, pos_f, speed_f, self, 0.f);
	}

	// Collision detection
	int loopcount = 0;

	bool previously_inside = false;

	while (dtime > BS * 1e-10f) {
		ScopeProfiler sp2(g_profiler, "collisionMovePoint dtime loop avg", SPT_AVG);

		// Avoid infinite loop
		loopcount++;
		if (loopcount >= 100) {
			warningstream << "collisionMovePoint: Loop count exceeded,"
				" aborting to avoid infiniite loop" << std::endl;
			break;
		}

		int nearest_collided = -1;
		f32 nearest_dtime = dtime;
		int nearest_boxindex = -1;

		// Go through every nodebox in path, find nearest collision

		// Collect node boxes in movement range
		std::vector<NearbyCollisionInfo> cinfo = cinfo_objects;

		bool any_position_valid = false;
		int loopcount_collect = 0;
		v3s16 p = floatToInt(*pos_f, BS);
		f32 dtime_collect = 0.f;
		v3f pos_test = *pos_f;

		bool finished = false;
		while (true) {
			++loopcount_collect;
			if (loopcount_collect >= 100) {
				warningstream << "collisionMovePoint collection"
					" node count exceeded, aborting to avoid"
					" infinite loop" << std::endl;
				break;
			}

			any_position_valid |= collect_node_nodeboxes(cinfo, map, p, gamedef);

			if (finished)
				break;

			// Find the next node to add

			using nl = std::numeric_limits<f32>;
			f32 max_value = nl::has_infinity ? nl::infinity() : nl::max();
			f32 dtime_X, dtime_Y, dtime_Z;

			dtime_X = dtime_Y = dtime_Z = max_value;

			v3f pos_node_center = intToFloat(floatToInt(pos_test, BS), BS);

			const auto calc_dtime = [&] (f32 pc, f32 pt, f32 speed)
			{
				f32 dtime = (pc - pt + (speed > 0 ? BS / 2.f : -BS / 2.f)) / speed;
				// Prevent infinite loop by floating point errors
				if (pt + speed*dtime == pt) {
					if (speed > 0)
						dtime = BS / speed;
					else
						dtime = -BS / speed;
				}
				return dtime;
			};

			if (speed_f->X != 0)
				dtime_X = calc_dtime(pos_node_center.X, pos_test.X, speed_f->X);
			if (speed_f->Y != 0)
				dtime_Y = calc_dtime(pos_node_center.Y, pos_test.Y, speed_f->Y);
			if (speed_f->Z != 0)
				dtime_Z = calc_dtime(pos_node_center.Z, pos_test.Z, speed_f->Z);

			f32 dtime_min = MYMIN(dtime_X, MYMIN(dtime_Y, dtime_Z));

			if (dtime_min == max_value)
				break;

			dtime_collect += dtime_min;

			/*
			 * Check in which direction the nearest node border is.
			 * If not near node border within dtime, do not add next node.
			 */
			if (dtime_min == dtime_X) {
				f32 border_dist = std::fabs(std::fmod(pos_test.X, BS));
				bool near_border;
				if (speed_f->X > 0) {
					near_border = (dtime_collect > dtime &&
						border_dist > 0.5*BS+surface_dist) ||
						(dtime_collect - dtime) *
							fabs(speed_f->X) > surface_dist;
				} else {
					near_border = (dtime_collect > dtime &&
						border_dist < 0.5*BS-surface_dist) ||
						(dtime_collect - dtime) *
						fabs(speed_f->X) > surface_dist;
				}
				if (dtime_collect > dtime && near_border)
						break;

				if (speed_f->X > 0)
					++p.X;
				else
					--p.X;

			} else if (dtime_min == dtime_Y) {
				f32 border_dist = std::fabs(std::fmod(pos_test.Y, BS));
				bool near_border;
				if (speed_f->Y > 0) {
					near_border = (dtime_collect > dtime &&
						border_dist > 0.5*BS+surface_dist) ||
						(dtime_collect - dtime) *
							fabs(speed_f->Y) > surface_dist;
				} else {
					near_border = (dtime_collect > dtime &&
						border_dist < 0.5*BS-surface_dist) ||
						(dtime_collect - dtime) *
						fabs(speed_f->Y) > surface_dist;
				}
				if (dtime_collect > dtime && near_border)
						break;

				if (speed_f->Y > 0)
					++p.Y;
				else
					--p.Y;
			} else if (dtime_min == dtime_Z) {
				f32 border_dist = std::fabs(std::fmod(pos_test.Z, BS));
				bool near_border;
				if (speed_f->Z > 0) {
					near_border = (dtime_collect > dtime &&
						border_dist > 0.5*BS+surface_dist) ||
						(dtime_collect - dtime) *
							fabs(speed_f->Z) > surface_dist;
				} else {
					near_border = (dtime_collect > dtime &&
						border_dist < 0.5*BS-surface_dist) ||
						(dtime_collect - dtime) *
						fabs(speed_f->Z) > surface_dist;
				}
				if (dtime_collect > dtime && near_border)
						break;

				if (speed_f->Z > 0)
					++p.Z;
				else
					--p.Z;
			}

			pos_test += dtime_min * (*speed_f);

			// Add next node anyway to keep a minimal distance of surface_dist
			if (dtime_collect > dtime)
				finished = true;
		}


		/*
		 * Do not collide if world has not loaded yet, since custom node boxes
		 * are not available for collision detection.
		 * This also intentionally occurs in the case of the object being positioned
		 * solely on loaded CONTENT_IGNORE nodes, no matter where they come from.
		 * The behavior is different from collisionMoveSimple.
		 */
		if (!any_position_valid) {
			return result;
		}

		{

		bool any_inside = false;
		for (u32 boxindex = 0; boxindex < cinfo.size(); boxindex++) {
			const NearbyCollisionInfo &box_info = cinfo[boxindex];

			// Find nearest collision of the two boxes (raytracing-like)
			f32 dtime_tmp;
			int collided;
			bool inside;
			std::tie(collided, dtime_tmp, inside) =
				pointBoxCollision(box_info.box,
				*pos_f, *speed_f, surface_dist);

			any_inside |= inside;

			if (collided == -1 || dtime_tmp >= nearest_dtime)
				continue;

			nearest_dtime = dtime_tmp;
			nearest_collided = collided;
			nearest_boxindex = boxindex;
		}

		if ((any_inside && previously_inside) || nearest_collided == -1) {
			// No collision with any collision box.
			*pos_f += *speed_f * dtime;
			dtime = 0; // Set to 0 to avoid "infinite" loop due to small FP numbers
		} else {
			// Otherwise, a collision occurred.
			NearbyCollisionInfo &nearest_info = cinfo[nearest_boxindex];

			*pos_f += *speed_f * nearest_dtime;
			if (nearest_dtime >= 0)
				dtime -= nearest_dtime;

			bool is_collision = true;
			if (nearest_info.is_unloaded)
				is_collision = false;

			CollisionInfo info;
			if (nearest_info.is_object)
				info.type = COLLISION_OBJECT;
			else
				info.type = COLLISION_NODE;

			info.node_p = nearest_info.position;
			info.old_speed = *speed_f;
			info.plane = nearest_collided;

			bool below_threshold = false;
			f32 bounce_thresholdSQ = bounce_threshold*bounce_threshold;
			// Bounce the particle with the speed component that caused the collision
			if (nearest_collided == 0) { // X
				speed_f->X *= -bounce_fraction;
				f32 speed_absSQ = speed_f->getLengthSQ();
				if (speed_f->X * speed_f->X <
						bounce_thresholdSQ*speed_absSQ)
					below_threshold = true;
			} else if (nearest_collided == 1) { // Y
				speed_f->Y *= -bounce_fraction;
				f32 speed_absSQ = speed_f->getLengthSQ();
				if (speed_f->Y * speed_f->Y <
						bounce_thresholdSQ * speed_absSQ)
					below_threshold = true;
			} else if (nearest_collided == 2) { // Z
				speed_f->Z *= -bounce_fraction;
				f32 speed_absSQ = speed_f->getLengthSQ();
				if (speed_f->Z * speed_f->Z <
						bounce_thresholdSQ * speed_absSQ)
					below_threshold = true;
			}
			result.collides = true;
			if (below_threshold)
				*speed_f = {0.f, 0.f, 0.f};

			info.new_speed = *speed_f;
			if (info.new_speed.getDistanceFrom(info.old_speed) < 0.1f * BS)
				is_collision = false;

			if (is_collision)
				result.collisions.push_back(info);

		}
		previously_inside = any_inside;

		}
	}

	return result;
}
