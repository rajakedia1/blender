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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __HAIR_SOLVER_H__
#define __HAIR_SOLVER_H__

extern "C" {
#include "DNA_hair_types.h"
}

#include "HAIR_curve.h"
#include "HAIR_memalloc.h"

HAIR_NAMESPACE_BEGIN

struct SolverData {
	SolverData();
	SolverData(int totcurves, int totpoints);
	SolverData(const SolverData &other);
	~SolverData();
	
	Curve *curves;
	Point *points;
	int totcurves;
	int totpoints;
	
	HAIR_CXX_CLASS_ALLOC(SolverData)
};

class Solver
{
public:
	Solver(const HairParams &params);
	~Solver();
	
	const HairParams &params() const { return m_params; }
	
	void init_data(int totcurves, int totpoints);
	void free_data();
	SolverData *data() const { return m_data; }
	
	void step(float timestep);
	
protected:
	float3 calc_velocity(Curve *curve, Point *point, float time, Point::State &state) const;
	float3 calc_acceleration(Curve *curve, Point *point, float time, Point::State &state) const;
	
private:
	HairParams m_params;
	SolverData *m_data;

	HAIR_CXX_CLASS_ALLOC(Solver)
};

HAIR_NAMESPACE_END

#endif
