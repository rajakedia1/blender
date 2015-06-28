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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/source/blenkernel/intern/effect_llvm.cpp
 *  \ingroup bke
 */

#include "llvm/Analysis/Passes.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/PassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_effect.h"
}

using namespace llvm;
using legacy::FunctionPassManager;
using legacy::PassManager;

typedef struct EffectorFunction EffectorFunction;

static Module *build_effector_module(EffectorCache *eff)
{
	return NULL;
}

static ExecutionEngine *create_execution_engine(Module *mod)
{
	std::string error;
	
	ExecutionEngine *engine = EngineBuilder(mod)
	                          .setErrorStr(&error)
//	                          .setMCJITMemoryManager(std::unique_ptr<HelpingMemoryManager>(
//	                                                     new HelpingMemoryManager(this)))
	                          .create();
	if (!engine) {
		fprintf(stderr, "Could not create ExecutionEngine: %s\n", error.c_str());
	}
	return engine;
}

static PassManager *create_pass_manager(ExecutionEngine *UNUSED(engine))
{
	PassManager *pm = new PassManager();
	
	// Set up the optimizer pipeline.  Start with registering info about how the
	// target lays out data structures.
//	pm->add(new DataLayout(*engine->getDataLayout()));
	// Provide basic AliasAnalysis support for GVN.
//	pm->add(createBasicAliasAnalysisPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
//	pm->add(createInstructionCombiningPass());
	// Reassociate expressions.
//	pm->add(createReassociatePass());
	// Eliminate Common SubExpressions.
//	pm->add(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
//	pm->add(createCFGSimplificationPass());
	
//	pm->doInitialization();
	
	return pm;
}

static Function *codegen(EffectorContext *effctx, Module *mod)
{
//	std::vector<Type*> args(2, Type::getFloatTy(getGlobalContext()));
//	FunctionType *functype = FunctionType::get(Type::getInt8Ty(getGlobalContext()), args, false);
	std::vector<Type*> args;
	FunctionType *functype = FunctionType::get(Type::getInt8Ty(getGlobalContext()), args, false);
	Function *func = Function::Create(functype, Function::ExternalLinkage, "MyFunction", mod);
	return func;
}

#if 0
static const char *ir_test_function =
"	.file	\"hello_world.c\"\n"
"	\n"
"	.ident	\"GCC: (Ubuntu 4.8.4-1ubuntu15) 4.8.4 LLVM: 3.4.2\"\n"
"	\n"
"	\n"
"		.text\n"
"		.align	16, 0x90\n"
"		.type	test,@function\n"
"test:\n"
"	.cfi_startproc\n"
"		pushq	%rbp\n"
"	.Ltmp2:\n"
"		.cfi_def_cfa_offset 16\n"
"	.Ltmp3:\n"
"		.cfi_offset %rbp, -16\n"
"		movq	%rsp, %rbp\n"
"	.Ltmp4:\n"
"		.cfi_def_cfa_register %rbp\n"
"		subq	$16, %rsp\n"
"		leaq	.cst, %rdi\n"
"		callq	puts\n"
"		movl	%eax, -4(%rbp)\n"
"		addq	$16, %rsp\n"
"		popq	%rbp\n"
"		ret\n"
"	.Ltmp5:\n"
"		.size	test, .Ltmp5-test\n"
"		.cfi_endproc\n"
"	\n"
"		.type	.cst,@object\n"
"		.section	.rodata,\"a\",@progbits\n"
"		.align	8\n"
"	.cst:\n"
"		.asciz	\"Hello World!\"\n"
"		.size	.cst, 13\n"
"	\n"
"	\n"
"		.section	\".note.GNU-stack\",\"\",@progbits\n";
#endif
static const char *ir_test_function =
"@.str = private constant [13 x i8] c\"Hello World!\\00\", align 1 ;\n"
"\n"
"define i32 @test() ssp {\n"
"entry:\n"
"  %retval = alloca i32\n"
"  %0 = alloca i32\n"
"  %\"alloca point\" = bitcast i32 0 to i32\n"
"  %1 = call i32 @puts(i8* getelementptr inbounds ([13 x i8]* @.str, i64 0, i64 0))\n"
"  store i32 0, i32* %0, align 4\n"
"  %2 = load i32* %0, align 4\n"
"  store i32 %2, i32* %retval, align 4\n"
"  br label %return\n"
"return:\n"
"  %retval1 = load i32* %retval\n"
"  ret i32 %retval1\n"
"}\n"
"\n"
"declare i32 @puts(i8*)\n";

void BKE_effect_build_function(EffectorContext *effctx)
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	LLVMContext &llvmctx = getGlobalContext();
	raw_ostream &errstream = errs();
	const char *entryname = "test";
	
	effctx->eval = NULL;
	
//	for (EffectorCache *eff = (EffectorCache *)effctx->effectors.first; eff; eff = eff->next) {
//		Module *effmod = build_effector_module();
//	}
	
#if 0
	Module* mod = new Module(entryname, llvmctx);
	Function *func = codegen(effctx, mod);
#else
	MemoryBuffer *buffer = MemoryBuffer::getMemBuffer(ir_test_function);
	SMDiagnostic err;
	
	printf("--- parsing ---\n");
	printf("%s", ir_test_function);
	printf("---------------\n");
	
	Module *mod = ParseIR(buffer, err, llvmctx);
	if (!mod) {
		err.print(entryname, errstream);
		return;
	}
	
	Function *func = mod->getFunction(entryname);
	if (!func) {
		printf("Could not find function %s\n", entryname);
//		mod->print(errstream, AssemblyAnnotationWriter);
		Module::FunctionListType::const_iterator it;
		for (it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
			printf("  %s\n", it->getName().str().c_str());
		}
		return;
	}
#endif
	
	verifyModule(*mod);
	
	ExecutionEngine *engine = create_execution_engine(mod);
	
	PassManager *pm = create_pass_manager(engine);
	pm->run(*mod);
	delete pm;
	
	effctx->eval = (EffectorEvalFp)engine->getPointerToFunction(func);
}
