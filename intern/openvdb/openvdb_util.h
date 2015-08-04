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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_UTIL_H__
#define __OPENVDB_UTIL_H__

#include <cstdio>
#include <string>

#include <openvdb/openvdb.h>

double time_dt();

/* A utility class which prints the time elapsed during its lifetime, useful for
 * e.g. timing the overall execution time of a function */
class ScopeTimer {
	double m_start;
	std::string m_message;

public:
	ScopeTimer(const std::string &message)
	    : m_message(message)
	{
		m_start = time_dt();
	}

	~ScopeTimer()
	{
		printf("%s: %fs\n", m_message.c_str(), time_dt() - m_start);
	}
};

#ifndef NDEBUG
#	define Timer(x) \
		ScopeTimer func(x);
#else
#	define Timer(x)
#endif

namespace internal {

using namespace openvdb;

static void copy_v3_v3(float *r, const float *a)
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

static void cross_v3_v3v3(float *r, const float *a, const float *b)
{
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
}

static void get_normal(float nor[3], const Vec3f &v1, const Vec3f &v2, const Vec3f &v3, const Vec3f &v4)
{
	openvdb::Vec3f n1, n2;

	n1 = v1 - v3;
	n2 = v2 - v4;

	cross_v3_v3v3(nor, n1.asV(), n2.asV());
}

static inline void add_quad(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                            const Vec3f &p1, const Vec3f &p2, const Vec3f &p3, const Vec3f &p4, const Vec3f &color)
{
	copy_v3_v3(verts[*verts_ofs+0], p1.asV());
	copy_v3_v3(verts[*verts_ofs+1], p2.asV());
	copy_v3_v3(verts[*verts_ofs+2], p3.asV());
	copy_v3_v3(verts[*verts_ofs+3], p4.asV());
	
	copy_v3_v3(colors[*verts_ofs+0], color.asV());
	copy_v3_v3(colors[*verts_ofs+1], color.asV());
	copy_v3_v3(colors[*verts_ofs+2], color.asV());
	copy_v3_v3(colors[*verts_ofs+3], color.asV());
	
	if (normals) {
		float nor[3];
		
		get_normal(nor, p1, p2, p3, p4);
		
		copy_v3_v3(normals[*verts_ofs+0], nor);
		copy_v3_v3(normals[*verts_ofs+1], nor);
		copy_v3_v3(normals[*verts_ofs+2], nor);
		copy_v3_v3(normals[*verts_ofs+3], nor);
	}
	
	*verts_ofs += 4;
}

static void add_box(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                    const openvdb::Vec3f &min, const openvdb::Vec3f &max,
                    const openvdb::Vec3f &color)
{
	using namespace openvdb;
	
	const Vec3f corners[8] = {
	    min,
	    Vec3f(min.x(), min.y(), max.z()),
	    Vec3f(max.x(), min.y(), max.z()),
	    Vec3f(max.x(), min.y(), min.z()),
	    Vec3f(min.x(), max.y(), min.z()),
	    Vec3f(min.x(), max.y(), max.z()),
	    max,
	    Vec3f(max.x(), max.y(), min.z())
	};

	add_quad(verts, colors, normals, verts_ofs, corners[0], corners[1], corners[2], corners[3], color);
	add_quad(verts, colors, normals, verts_ofs, corners[7], corners[6], corners[5], corners[4], color);
	add_quad(verts, colors, normals, verts_ofs, corners[4], corners[5], corners[1], corners[0], color);
	add_quad(verts, colors, normals, verts_ofs, corners[3], corners[2], corners[6], corners[7], color);
	add_quad(verts, colors, normals, verts_ofs, corners[3], corners[7], corners[4], corners[0], color);
	add_quad(verts, colors, normals, verts_ofs, corners[1], corners[5], corners[6], corners[2], color);
}

template <typename TreeType>
static void OpenVDB_get_draw_buffer_size_cells(openvdb::Grid<TreeType> *grid, int min_level, int max_level, bool voxels,
                                               int *r_numverts)
{
	using namespace openvdb;
	using namespace openvdb::math;
	
	typedef typename TreeType::LeafNodeType LeafNodeType;
	
	if (!grid) {
		*r_numverts = 0;
		return;
	}
	
	/* count */
	int numverts = 0;
	for (typename TreeType::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
		const int level = node_iter.getLevel();
		
		if (level < min_level || level > max_level)
			continue;
		
		numverts += 6 * 4;
	}
	
	if (voxels) {
		for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
			const LeafNodeType *leaf = leaf_iter.getLeaf();
			
			numverts += 6 * 4 * leaf->onVoxelCount();
		}
	}
	
	*r_numverts = numverts;
}

template <typename TreeType>
static void OpenVDB_get_draw_buffers_cells(openvdb::Grid<TreeType> *grid, int min_level, int max_level, bool voxels,
                                           float (*verts)[3], float (*colors)[3])
{
	using namespace openvdb;
	using namespace openvdb::math;
	
	typedef typename TreeType::LeafNodeType LeafNodeType;
	
	/* The following colors are meant to be the same as in the example images of
	 * "VDB: High-Resolution Sparse Volumes With Dynamic Topology", K. Museth, 2013
	 */
	static const Vec3f node_color[4] = {
	    Vec3f(0.045f, 0.045f, 0.045f), // root node (black)
	    Vec3f(0.043f, 0.330f, 0.041f), // first internal node level (green)
	    Vec3f(0.871f, 0.394f, 0.019f), // intermediate internal node levels (orange)
	    Vec3f(0.006f, 0.280f, 0.625f)  // leaf nodes (blue)
	};
	static const Vec3f voxel_color = Vec3f(1.000f, 0.000f, 0.000f); // active voxel (red)
	
	if (!grid)
		return;
	
	int verts_ofs = 0;
	for (typename TreeType::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
		const int level = node_iter.getLevel();
		
		if (level < min_level || level > max_level)
			continue;
		
		CoordBBox bbox;
		node_iter.getBoundingBox(bbox);
		
		const Vec3f min(bbox.min().x() - 0.5f, bbox.min().y() - 0.5f, bbox.min().z() - 0.5f);
		const Vec3f max(bbox.max().x() + 0.5f, bbox.max().y() + 0.5f, bbox.max().z() + 0.5f);
		
		Vec3f wmin = grid->indexToWorld(min);
		Vec3f wmax = grid->indexToWorld(max);
		
		Vec3f color = node_color[std::max(3 - level, 0)];
		
		add_box(verts, colors, NULL, &verts_ofs, wmin, wmax, color);
	}
	
	if (voxels) {
		for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
			const LeafNodeType *leaf = leaf_iter.getLeaf();
			
			for (typename LeafNodeType::ValueOnCIter value_iter = leaf->cbeginValueOn(); value_iter; ++value_iter) {
				const Coord ijk = value_iter.getCoord();
				
				Vec3f min(ijk.x() - 0.5f, ijk.y() - 0.5f, ijk.z() - 0.5f);
				Vec3f max(ijk.x() + 0.5f, ijk.y() + 0.5f, ijk.z() + 0.5f);
				Vec3f wmin = grid->indexToWorld(min);
				Vec3f wmax = grid->indexToWorld(max);
				
				Vec3f color = voxel_color;
				
				add_box(verts, colors, NULL, &verts_ofs, wmin, wmax, color);
			}
		}
	}
}

template <typename T>
struct FloatConverter {
	static float get(T value)
	{
		return value;
	}
};

template <typename TreeType>
static void OpenVDB_get_draw_buffer_size_boxes(openvdb::Grid<TreeType> *grid,
                                               int *r_numverts)
{
	using namespace openvdb;
	using namespace openvdb::math;
	
	typedef typename TreeType::LeafNodeType LeafNodeType;
	
	if (!grid) {
		*r_numverts = 0;
		return;
	}
	
	/* count */
	int numverts = 0;
	for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
		const LeafNodeType *leaf = leaf_iter.getLeaf();
		
		numverts += 6 * 4 * leaf->onVoxelCount();
	}
	
	*r_numverts = numverts;
}

static inline void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b)
{
	float nr, ng, nb;

	nr =        fabs(h * 6.0f - 3.0f) - 1.0f;
	ng = 2.0f - fabs(h * 6.0f - 2.0f);
	nb = 2.0f - fabs(h * 6.0f - 4.0f);

	nr = std::max(0.0f, std::min(nr, 1.0f));
	ng = std::max(0.0f, std::min(ng, 1.0f));
	nb = std::max(0.0f, std::min(nb, 1.0f));

	*r = ((nr - 1.0f) * s + 1.0f) * v;
	*g = ((ng - 1.0f) * s + 1.0f) * v;
	*b = ((nb - 1.0f) * s + 1.0f) * v;
}

template <typename TreeType>
static void OpenVDB_get_draw_buffers_boxes(openvdb::Grid<TreeType> *grid,
                                           float (*verts)[3], float (*colors)[3], float (*normals)[3])
{
	using namespace openvdb;
	using namespace openvdb::math;
	
	typedef typename TreeType::ValueType ValueType;
	typedef typename TreeType::LeafNodeType LeafNodeType;
	
	if (!grid)
		return;
	
	int verts_ofs = 0;
	
	for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
		const LeafNodeType *leaf = leaf_iter.getLeaf();
		
		for (typename LeafNodeType::ValueOnCIter value_iter = leaf->cbeginValueOn(); value_iter; ++value_iter) {
			const Coord ijk = value_iter.getCoord();
			
			float fac = FloatConverter<ValueType>::get(value_iter.getValue());
			fac = std::max(0.0f, std::min(-fac, 1.0f));
			
			Vec3f min(ijk.x() - 0.5f*fac, ijk.y() - 0.5f*fac, ijk.z() - 0.5f*fac);
			Vec3f max(ijk.x() + 0.5f*fac, ijk.y() + 0.5f*fac, ijk.z() + 0.5f*fac);
			Vec3f wmin = grid->indexToWorld(min);
			Vec3f wmax = grid->indexToWorld(max);
			
			float r, g, b;
			hsv_to_rgb((fac + 2.0f) / 3.0f, 1.0f, 1.0f, &r, &g, &b);
			Vec3f color = Vec3f(r, g, b);
			
			add_box(verts, colors, normals, &verts_ofs, wmin, wmax, color);
		}
	}
}

} /* namespace internal */

#endif /* __OPENVDB_UTIL_H__ */
