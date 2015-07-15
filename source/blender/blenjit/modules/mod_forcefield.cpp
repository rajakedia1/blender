/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "stdio.h"
#include "math.h"

#include <stdbool.h>

#include "mod_common.h"

#include "bjit_util_dualmath.h"
#include "bjit_util_math.h"

using namespace bjit;

/* maxdist: zero effect from this distance outwards (if usemax) */
/* mindist: full effect up to this distance (if usemin) */
/* power: falloff with formula 1/r^power */
static float get_falloff_old(float fac, bool usemin, float mindist, bool usemax, float maxdist, float power)
{
	/* first quick checks */
	if (usemax && fac > maxdist)
		return 0.0f;

	if (usemin && fac < mindist)
		return 1.0f;

	if (!usemin)
		mindist = 0.0;

	return pow((double)(1.0f+fac-mindist), (double)(-power));
}

static float get_falloff(float distance, bool usemin, float mindist, bool usemax, float maxdist, float power)
{
	return get_falloff_old(distance, usemin, mindist, usemax, maxdist, power);
}

/* ------------------------------------------------------------------------- */

/* relation of a point to the effector,
 * based on type, shape, etc.
 */
typedef struct EffectorPointRelation {
	/* closest on the effector */
	vec3_t closest_loc;
	vec3_t closest_nor;
	vec3_t closest_vel;
	
	/* relative coordinates of the point */
	vec3_t loc_rel;
	float dist_rel;
} EffectorPointRelation;

bool get_point_relation(EffectorPointRelation *rel, vec3_t loc, vec3_t /*vel*/,
                        mat4_t transform, int shape)
{
	switch (shape) {
		case EFF_FIELD_SHAPE_POINT: {
			/* use center of object for distance calculus */
			copy_v3_v3(rel->closest_loc, transform[3]);
						
			/* use z-axis as normal*/
			normalize_v3_v3(rel->closest_nor, transform[2]);
			
			// TODO
			zero_v3(rel->closest_vel);
			
			break;
		}
		case EFF_FIELD_SHAPE_PLANE: {
			float center[3], locrel[3], offset[3];
			
			/* use z-axis as normal*/
			normalize_v3_v3(rel->closest_nor, transform[2]);
			
			/* use center of object for distance calculus */
			copy_v3_v3(center, transform[3]);
			/* radial offset */
			sub_v3_v3v3(locrel, loc, center);
			project_plane_v3_v3v3(offset, locrel, rel->closest_nor);
			add_v3_v3v3(rel->closest_loc, center, offset);
			
			// TODO
			zero_v3(rel->closest_vel);
			
			break;
		}
		case EFF_FIELD_SHAPE_SURFACE: {
			return false; // TODO
			break;
		}
		case EFF_FIELD_SHAPE_POINTS: {
			return false; // TODO
			break;
		}
		
		default:
			return false;
	}
	
	sub_v3_v3v3(rel->loc_rel, loc, rel->closest_loc);
	rel->dist_rel = len_v3(rel->loc_rel);
	
	return true;
}

__attribute__((annotate("effector_force_eval")))
EffectorEvalResult effector_force_eval(const vec3_t &loc, const vec3_t &vel,
                                       mat4_t transform, int shape,
                                       float strength, float power)
{
	EffectorEvalResult result;
	
	EffectorPointRelation rel;
	get_point_relation(&rel, loc, vel, transform, shape);
	float factor = get_falloff(rel.dist_rel, false, 0.0f, false, 1.0f, power);
	
	vec3_t dir;
	normalize_v3_v3(dir, rel.loc_rel);
	mul_v3_v3fl(result.force, dir, strength * factor);
	
	zero_v3(result.impulse);
	
	dual_f test(0.2);
	dual_f test2(4,5,6,6);
	test += test2;
	printf("test %f\n", (float)test);
	
	return result;
}
