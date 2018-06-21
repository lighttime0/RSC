#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstIterator.h"

using namespace llvm;

namespace {
struct RSC : public FunctionPass {
	static char ID;
	RSC() : FunctionPass(ID) {}

	bool runOnFunction(Function &F) override {
		int ins_counter = 0;
		for (BasicBlock &B : F) {
			for (Instruction &I: B) {
				++ins_counter;
			}
		}
		errs() << "There are " << ins_counter << " instructions in ";
		errs().write_escaped(F.getName()) << '\n';
		return false;
	}
}; // end of struct RSC
}  // end of anonymous namespace

char RSC::ID = 0;
static RegisterPass<RSC> X("RSC-demo", "This is a RSC demo",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);