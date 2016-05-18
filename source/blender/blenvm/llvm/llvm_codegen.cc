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

/** \file blender/blenvm/llvm/llvm_codegen.cc
 *  \ingroup llvm
 */

#include <cstdio>
#include <set>
#include <sstream>

#include "node_graph.h"

#include "llvm_codegen.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"
#include "llvm_modules.h"
#include "llvm_types.h"

namespace blenvm {

LLVMCompilerBase::LLVMCompilerBase() :
    m_module(NULL)
{
}

LLVMCompilerBase::~LLVMCompilerBase()
{
}

llvm::LLVMContext &LLVMCompilerBase::context() const
{
	return llvm::getGlobalContext();
}

void LLVMCompilerBase::create_module(const string &name)
{
	m_module = new llvm::Module(name, context());
}

void LLVMCompilerBase::destroy_module()
{
	delete m_module;
	m_module = NULL;
}

llvm::Constant *LLVMCompilerBase::codegen_constant(const NodeValue *node_value)
{
	using namespace llvm;
	
	const TypeSpec *typespec = node_value->typedesc().get_typespec();
	if (typespec->is_structure()) {
//		const StructSpec *s = typespec->structure();
		/* TODO don't have value storage for this yet */
		return NULL;
	}
	else {
		switch (typespec->base_type()) {
			case BVM_FLOAT: {
				float f = 0.0f;
				node_value->get(&f);
				return ConstantFP::get(context(), APFloat(f));
			}
			case BVM_FLOAT3: {
				StructType *stype = TypeBuilder<float3, true>::get(context());
				
				float3 f = float3(0.0f, 0.0f, 0.0f);
				node_value->get(&f);
				return ConstantStruct::get(stype,
				                           ConstantFP::get(context(), APFloat(f.x)),
				                           ConstantFP::get(context(), APFloat(f.y)),
				                           ConstantFP::get(context(), APFloat(f.z)),
				                           NULL);
			}
			case BVM_FLOAT4: {
				StructType *stype = TypeBuilder<float4, true>::get(context());
				
				float4 f = float4(0.0f, 0.0f, 0.0f, 0.0f);
				node_value->get(&f);
				return ConstantStruct::get(stype,
				                           ConstantFP::get(context(), APFloat(f.x)),
				                           ConstantFP::get(context(), APFloat(f.y)),
				                           ConstantFP::get(context(), APFloat(f.z)),
				                           ConstantFP::get(context(), APFloat(f.w)),
				                           NULL);
			}
			case BVM_INT: {
				int i = 0;
				node_value->get(&i);
				return ConstantInt::get(context(), APInt(32, i, true));
			}
			case BVM_MATRIX44: {
				Type *elem_t = TypeBuilder<types::ieee_float, true>::get(context());
				ArrayType *inner_t = ArrayType::get(elem_t, 4);
				ArrayType *outer_t = ArrayType::get(inner_t, 4);
				StructType *matrix_t = StructType::get(outer_t, NULL);
				
				matrix44 m = matrix44::identity();
				node_value->get(&m);
				Constant *constants[4][4];
				for (int i = 0; i < 4; ++i)
					for (int j = 0; j < 4; ++j)
						constants[i][j] = ConstantFP::get(context(), APFloat(m.data[i][j]));
				Constant *cols[4];
				for (int i = 0; i < 4; ++i)
					cols[i] = ConstantArray::get(inner_t, ArrayRef<Constant*>(constants[i], 4));
				Constant *data = ConstantArray::get(outer_t, ArrayRef<Constant*>(cols, 4));
				return ConstantStruct::get(matrix_t,
				                           data, NULL);
			}
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				return NULL;
		}
	}
	return NULL;
}

void LLVMCompilerBase::codegen_node(llvm::BasicBlock *block,
                                    const NodeInstance *node)
{
	switch (node->type->kind()) {
		case NODE_TYPE_FUNCTION:
		case NODE_TYPE_KERNEL:
			expand_function_node(block, node);
			break;
		case NODE_TYPE_PASS:
			expand_pass_node(block, node);
			break;
		case NODE_TYPE_ARG:
			expand_argument_node(block, node);
			break;
	}
}

/* Compile nodes as a simple expression.
 * Every node can be treated as a single statement. Each node is translated
 * into a function call, with regular value arguments. The resulting value is
 * assigned to a variable and can be used for subsequent node function calls.
 */
llvm::BasicBlock *LLVMCompilerBase::codegen_function_body_expression(const NodeGraph &graph, llvm::Function *func)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	
	BasicBlock *block = BasicBlock::Create(context(), "entry", func);
	builder.SetInsertPoint(block);
	
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	
	Argument *retarg = func->getArgumentList().begin();
	{
		Function::ArgumentListType::iterator it = retarg;
		for (int i = 0; i < num_outputs; ++i) {
			++it; /* skip output arguments */
		}
		for (int i = 0; i < num_inputs; ++i) {
			const NodeGraph::Input &input = graph.inputs[i];
			Argument *arg = &(*it++);
			
			map_argument(block, input.key, arg);
		}
	}
	
	OrderedNodeSet nodes;
	for (NodeGraph::NodeInstanceMap::const_iterator it = graph.nodes.begin(); it != graph.nodes.end(); ++it)
		nodes.insert(it->second);
	
	for (OrderedNodeSet::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		codegen_node(block, &node);
	}
	
	{
		Function::ArgumentListType::iterator it = func->getArgumentList().begin();
		for (int i = 0; i < num_outputs; ++i) {
			const NodeGraph::Output &output = graph.outputs[i];
			Argument *arg = &(*it++);
			
			store_return_value(block, output.key, arg);
		}
	}
	
	builder.CreateRetVoid();
	
	return block;
}

llvm::Function *LLVMCompilerBase::codegen_node_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	std::vector<llvm::Type*> input_types, output_types;
	for (int i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		const string &tname = input.typedesc.name();
		const TypeSpec *typespec = input.typedesc.get_typespec();
		Type *type = llvm_create_value_type(context(), tname, typespec);
		if (llvm_use_argument_pointer(typespec))
			type = type->getPointerTo();
		input_types.push_back(type);
	}
	for (int i = 0; i < graph.outputs.size(); ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		const string &tname = output.typedesc.name();
		const TypeSpec *typespec = output.typedesc.get_typespec();
		Type *type = llvm_create_value_type(context(), tname, typespec);
		output_types.push_back(type);
	}
	FunctionType *functype = llvm_create_node_function_type(context(), input_types, output_types);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, module());
	
	BLI_assert(func->getArgumentList().size() == graph.inputs.size() + graph.outputs.size() &&
	           "Error: Function has wrong number of arguments for node tree\n");
	
	codegen_function_body_expression(graph, func);
	
	return func;
}

void LLVMCompilerBase::optimize_function(llvm::Function *func, int opt_level)
{
	using namespace llvm;
	using legacy::FunctionPassManager;
	using legacy::PassManager;
	
	FunctionPassManager FPM(module());
	PassManager MPM;
	
#if 0
	/* Set up the optimizer pipeline.
	 * Start with registering info about how the
	 * target lays out data structures.
	 */
	FPM.add(new DataLayoutPass(*llvm_execution_engine()->getDataLayout()));
	/* Provide basic AliasAnalysis support for GVN. */
	FPM.add(createBasicAliasAnalysisPass());
	/* Do simple "peephole" optimizations and bit-twiddling optzns. */
	FPM.add(createInstructionCombiningPass());
	/* Reassociate expressions. */
	FPM.add(createReassociatePass());
	/* Eliminate Common SubExpressions. */
	FPM.add(createGVNPass());
	/* Simplify the control flow graph (deleting unreachable blocks, etc). */
	FPM.add(createCFGSimplificationPass());
	
	FPM.doInitialization();
#endif
	
	PassManagerBuilder builder;
	builder.OptLevel = opt_level;
	
	builder.populateModulePassManager(MPM);
	if (opt_level > 1) {
		/* Inline small functions */
		MPM.add(createFunctionInliningPass());
	}
	
	if (opt_level > 1) {
		/* Optimize memcpy intrinsics */
		FPM.add(createMemCpyOptPass());
	}
	builder.populateFunctionPassManager(FPM);
	
	MPM.run(*module());
	FPM.run(*func);
}

/* ------------------------------------------------------------------------- */

void LLVMSimpleCompilerImpl::codegen_begin()
{
}

void LLVMSimpleCompilerImpl::codegen_end()
{
	m_output_values.clear();
}

void LLVMSimpleCompilerImpl::map_argument(llvm::BasicBlock *block, const OutputKey &output, llvm::Argument *arg)
{
	m_output_values[output] = arg;
	UNUSED_VARS(block);
}

void LLVMSimpleCompilerImpl::store_return_value(llvm::BasicBlock *block, const OutputKey &output, llvm::Value *arg)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	Value *value = m_output_values.at(output);
	Value *rvalue = builder.CreateLoad(value);
	builder.CreateStore(rvalue, arg);
}

void LLVMSimpleCompilerImpl::expand_pass_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	BLI_assert(node->num_inputs() == 1);
	BLI_assert(node->num_outputs() == 1);
	
	ConstInputKey input = node->input(0);
	ConstOutputKey output = node->output(0);
	BLI_assert(input.value_type() == INPUT_EXPRESSION);
	
	Value *value = m_output_values.at(input.link());
	bool ok = m_output_values.insert(OutputValueMap::value_type(output, value)).second;
	BLI_assert(ok && "Value for node output already defined!");
	UNUSED_VARS(ok);
}

void LLVMSimpleCompilerImpl::expand_argument_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	/* input arguments are mapped in advance */
	BLI_assert(m_output_values.find(node->output(0)) != m_output_values.end() &&
	           "Input argument value node mapped!");
	UNUSED_VARS(block, node);
}

void LLVMSimpleCompilerImpl::expand_function_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* get evaluation function */
	const std::string &evalname = node->type->name();
	Function *evalfunc = llvm_find_external_function(module(), evalname);
	BLI_assert(evalfunc != NULL && "Could not find node function!");
	
	/* function call arguments (including possible return struct if MRV is used) */
	std::vector<Value *> args;
	
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		const string &tname = output.socket->typedesc.name();
		const TypeSpec *typespec = output.socket->typedesc.get_typespec();
		Type *type = llvm_create_value_type(context(), tname, typespec);
		BLI_assert(type != NULL);
		Value *value = builder.CreateAlloca(type);
		
		args.push_back(value);
		
		/* use as node output values */
		bool ok = m_output_values.insert(OutputValueMap::value_type(output, value)).second;
		BLI_assert(ok && "Value for node output already defined!");
		UNUSED_VARS(ok);
	}
	
	/* set input arguments */
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		const TypeSpec *typespec = input.socket->typedesc.get_typespec();
		
		switch (input.value_type()) {
			case INPUT_CONSTANT: {
				/* create storage for the global value */
				Constant *cvalue = codegen_constant(input.value());
				
				Value *value;
				if (llvm_use_argument_pointer(typespec)) {
					AllocaInst *pvalue = builder.CreateAlloca(cvalue->getType());
					builder.CreateStore(cvalue, pvalue);
					value = pvalue;
				}
				else {
					value = cvalue;
				}
				
				args.push_back(value);
				break;
			}
			case INPUT_EXPRESSION: {
				Value *pvalue = m_output_values.at(input.link());
				Value *value;
				if (llvm_use_argument_pointer(typespec)) {
					value = pvalue;
				}
				else {
					value = builder.CreateLoad(pvalue);
				}
				
				args.push_back(value);
				break;
			}
			case INPUT_VARIABLE: {
				/* TODO */
				BLI_assert(false && "Variable inputs not supported yet!");
				break;
			}
		}
	}
	
	CallInst *call = builder.CreateCall(evalfunc, args);
	UNUSED_VARS(call);
}

/* ------------------------------------------------------------------------- */

void LLVMTextureCompilerImpl::codegen_begin()
{
}

void LLVMTextureCompilerImpl::codegen_end()
{
	m_output_values.clear();
}

void LLVMTextureCompilerImpl::map_argument(llvm::BasicBlock *block, const OutputKey &output, llvm::Argument *arg)
{
	m_output_values[output] = arg;
	UNUSED_VARS(block);
}

void LLVMTextureCompilerImpl::store_return_value(llvm::BasicBlock *block, const OutputKey &output, llvm::Value *arg)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	Value *value = m_output_values.at(output);
	Value *rvalue = builder.CreateLoad(value);
	builder.CreateStore(rvalue, arg);
}

void LLVMTextureCompilerImpl::expand_pass_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	BLI_assert(node->num_inputs() == 1);
	BLI_assert(node->num_outputs() == 1);
	
	ConstInputKey input = node->input(0);
	ConstOutputKey output = node->output(0);
	BLI_assert(input.value_type() == INPUT_EXPRESSION);
	
	Value *value = m_output_values.at(input.link());
	bool ok = m_output_values.insert(OutputValueMap::value_type(output, value)).second;
	BLI_assert(ok && "Value for node output already defined!");
	UNUSED_VARS(ok);
}

void LLVMTextureCompilerImpl::expand_argument_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	/* input arguments are mapped in advance */
	BLI_assert(m_output_values.find(node->output(0)) != m_output_values.end() &&
	           "Input argument value node mapped!");
	UNUSED_VARS(block, node);
}

void LLVMTextureCompilerImpl::expand_function_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* get evaluation function */
	const std::string &evalname = node->type->name();
	Function *evalfunc = llvm_find_external_function(module(), evalname);
	BLI_assert(evalfunc != NULL && "Could not find node function!");
	
	/* function call arguments (including possible return struct if MRV is used) */
	std::vector<Value *> args;
	
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		const string &tname = output.socket->typedesc.name();
		const TypeSpec *typespec = output.socket->typedesc.get_typespec();
		Type *type = llvm_create_value_type(context(), tname, typespec);
		BLI_assert(type != NULL);
		Value *value = builder.CreateAlloca(type);
		
		args.push_back(value);
		
		/* use as node output values */
		bool ok = m_output_values.insert(OutputValueMap::value_type(output, value)).second;
		BLI_assert(ok && "Value for node output already defined!");
		UNUSED_VARS(ok);
	}
	
	/* set input arguments */
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		const TypeSpec *typespec = input.socket->typedesc.get_typespec();
		
		switch (input.value_type()) {
			case INPUT_CONSTANT: {
				/* create storage for the global value */
				Constant *cvalue = codegen_constant(input.value());
				
				Value *value;
				if (llvm_use_argument_pointer(typespec)) {
					AllocaInst *pvalue = builder.CreateAlloca(cvalue->getType());
					builder.CreateStore(cvalue, pvalue);
					value = pvalue;
				}
				else {
					value = cvalue;
				}
				
				args.push_back(value);
				break;
			}
			case INPUT_EXPRESSION: {
				Value *pvalue = m_output_values.at(input.link());
				Value *value;
				if (llvm_use_argument_pointer(typespec)) {
					value = pvalue;
				}
				else {
					value = builder.CreateLoad(pvalue);
				}
				
				args.push_back(value);
				break;
			}
			case INPUT_VARIABLE: {
				/* TODO */
				BLI_assert(false && "Variable inputs not supported yet!");
				break;
			}
		}
	}
	
	CallInst *call = builder.CreateCall(evalfunc, args);
	UNUSED_VARS(call);
}

/* ------------------------------------------------------------------------- */

FunctionLLVM *LLVMCompiler::compile_function(const string &name, const NodeGraph &graph, int opt_level)
{
	using namespace llvm;
	
	codegen_begin();
	
	create_module(name);
	llvm_link_module_full(module());
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	codegen_end();
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	optimize_function(func, opt_level);
	
#if 0
	printf("=== NODE FUNCTION ===\n");
	fflush(stdout);
//	func->dump();
	module()->dump();
	printf("=====================\n");
	fflush(stdout);
#endif
	
	verifyFunction(*func, &outs());
	verifyModule(*module(), &outs());
	
	/* Note: Adding module to exec engine before creating the function prevents compilation! */
	llvm_execution_engine()->addModule(module());
	llvm_execution_engine()->generateCodeForModule(module());
	uint64_t address = llvm_execution_engine()->getFunctionAddress(name);
	BLI_assert(address != 0);
	
	llvm_execution_engine()->removeModule(module());
	destroy_module();
	
	FunctionLLVM *fn = new FunctionLLVM(address);
	return fn;
}

/* ------------------------------------------------------------------------- */

/* llvm::ostream that writes to a FILE. */
class file_ostream : public llvm::raw_ostream {
	FILE *file;
	
	/* write_impl - See raw_ostream::write_impl. */
	void write_impl(const char *ptr, size_t size) override {
		fwrite(ptr, sizeof(char), size, file);
	}
	
	/* current_pos - Return the current position within the stream, not
	 * counting the bytes currently in the buffer.
	 */
	uint64_t current_pos() const override { return ftell(file); }
	
public:
	explicit file_ostream(FILE *f) : file(f) {}
	~file_ostream() {
		fflush(file);
	}
};

class debug_assembly_annotation_writer : public llvm::AssemblyAnnotationWriter
{
	/* add implementation here if needed */
};

void DebugLLVMCompiler::compile_function(const string &name, const NodeGraph &graph, int opt_level, FILE *file)
{
	using namespace llvm;
	
	create_module(name);
	llvm_link_module_full(module());
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	optimize_function(func, opt_level);
	
	file_ostream stream(file);
	debug_assembly_annotation_writer aaw;
	module()->print(stream, &aaw);
	
	destroy_module();
}

} /* namespace blenvm */