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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BVM_EVAL_H__
#define __BVM_EVAL_H__

/** \file bvm_eval.h
 *  \ingroup bvm
 */

#include <set>
#include <vector>

#include "MEM_guardedalloc.h"

extern "C" {
#include "RNA_access.h"
}

#include "util_map.h"
#include "util_string.h"

struct ID;
struct Image;
struct ImagePool;
struct ImageUser;
struct ImBuf;
struct Object;

namespace blenvm {

struct InstructionList;

#define BVM_STACK_SIZE 4095

struct EvalGlobals {
	typedef unordered_map<int, Object *> ObjectMap;
	typedef unordered_map<int, Image *> ImageMap;
	
	EvalGlobals();
	~EvalGlobals();
	
	static int get_id_key(ID *id);
	
	void add_object(int key, Object *ob);
	PointerRNA lookup_object(int key) const;
	
	void add_image(int key, Image *ima);
	ImBuf *lookup_imbuf(int key, ImageUser *iuser) const;
	
private:
	ObjectMap m_objects;
	ImageMap m_images;
	ImagePool *m_image_pool;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:EvalGlobals")
};

struct EvalStack {
	static int stack_size(size_t datasize);
	
	int value;
};

struct EvalContext {
	EvalContext();
	~EvalContext();
	
	void eval_expression(const EvalGlobals *globals, const InstructionList *instr, int entry_point, EvalStack *stack) const;
	
	void eval_instructions(const EvalGlobals *globals, const InstructionList *instr, int entry_point, EvalStack *stack) const;
	
	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:EvalContext")
};

} /* namespace blenvm */

#endif /* __BVM_EVAL_H__ */