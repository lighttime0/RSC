#include "FsigAnalysis.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

llvm::Value *pseudo_return = (llvm::Value *)(0xdead);

FsigAnalysis::FsigAnalysis(path_iterator &path)
	: path(path)
{
}

void FsigAnalysis::runOnBasicBlocks(const std::set<llvm::BasicBlock*> &bbs) {
	do {
		done = true;
		for (auto bb : bbs)
			visit(bb);
	} while (!done);
}

void FsigAnalysis::runOnPredicates(const std::list<llvm::Value*> &true_preds,
				   const std::list<llvm::Value*> &false_preds) {
	do {
		done = true;
		pred_holds = true;
		for (auto v : true_preds)
			visit(dyn_cast<Instruction>(v));
		pred_holds = false;
		for (auto v : false_preds)
			visit(dyn_cast<Instruction>(v));
	} while (!done);
}
