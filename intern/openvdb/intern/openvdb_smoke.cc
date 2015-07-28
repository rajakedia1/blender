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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <openvdb/tools/ValueTransformer.h>  /* for tools::foreach */

#include "openvdb_smoke.h"

namespace internal {

using namespace openvdb;
using namespace openvdb::math;

OpenVDBSmokeData::OpenVDBSmokeData()
{
}

OpenVDBSmokeData::~OpenVDBSmokeData()
{
}

void OpenVDBSmokeData::add_obstacle(Transform::Ptr &tfm, const std::vector<Vec3s> &vertices, const std::vector<Vec4I> &triangles)
{
	tools::MeshToVolume<FloatGrid> converter(tfm);
	
	converter.convertToLevelSet(vertices, triangles);
	
	density = converter.distGridPtr();
}

void OpenVDBSmokeData::clear_obstacles()
{
	if (density)
		density->clear();
}

bool OpenVDBSmokeData::step(float dt, int num_substeps)
{
	return true;
}

}  /* namespace internal */
