#include <cstdio>
#include <iostream>
#include <cstring>
#include <string>
#include <list>
#include <algorithm>

#include <boost/regex.hpp>

#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/CallGraphSCCPass.h>
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TimeValue.h>

#include "util.h"

using namespace llvm;
using namespace rsc;

 static cl::opt<int>
MaxPath("max-path-per-func",
	cl::init(100),
	cl::desc("Maximum number of paths to be enumerated in a function"));

static cl::opt<int>
MaxSubcase("max-subcase-per-path",
	   cl::init(10),
	   cl::desc("Maximum number of subcases to be enumerated in a path"));

static cl::opt<bool>
O_TEST("o-test",
       cl::init(false),
       cl::desc("Print final summaries for test"));

static cl::opt<bool>
O_PROGRESS("o-progress",
	   cl::init(false),
	   cl::desc("Print progress to stdout"));

static cl::opt<bool>
O_VERBOSE("o-verbose",
	   cl::init(false),
	   cl::desc("Print internal warnings"));

static cl::opt<std::string>
SINGLE_FN("single-fn",
	  cl::init(""),
	  cl::desc("Verbosely test the analysis of the given function"));

static cl::opt<std::string>
BLACKLIST("blacklist",
	  cl::init(""),
	  cl::desc("A list of function names which are excluded from the analysis"));

static cl::opt<std::string>
PLOT_CFG("plot-cfg",
	 cl::init(""),
	 cl::desc("plot the CFG when an inconsistent path pair is found, saving the files under the given directory"));

cl::opt<std::string>
SENSILIST("sensilist",
	  cl::init(""),
	  cl::desc("A list of functions that should be analyzed"));

class RSC : public CallGraphSCCPass {

	int progress, total;
	std::string progress_buf;
	raw_string_ostream progress_os;

	bool single_fn_mode;

	std::list<std::string> blacklist;
	std::list<std::string> sensilist;

	std::list<std::string> enter_atomic_context_functions;

	int ipp_id;

	void run_on_function(CallGraphNode *N, Function &F) {
		int nr_branches = 0;
		//Summary *summary = NULL;
		StringRef fn = getFunctionName(&F);

		int paths = 0, subcases = 0;

		std::cout << fn.str() << std::endl;
			
		for (BasicBlock &B : F) {
	        for (Instruction &I: B) {
				if (auto *CI = dyn_cast<CallInst>(&I)) {
					StringRef cnt_fn = getFunctionName(CI->getCalledFunction());
					std::string cnt_fn_name_str = cnt_fn.str();
					std::list<std::string>::iterator it = find(enter_atomic_context_functions.begin(), 
						enter_atomic_context_functions.end(), cnt_fn_name_str);
					if (it != enter_atomic_context_functions.end()) {
						//means fn is a function that can enter atomic context
						//do something
					}
				}
			}
		}
	}

public:
	static char ID;

	RSC() : CallGraphSCCPass(ID),
		progress_os(progress_buf),
		single_fn_mode(false),
		ipp_id(0)
		{}

	virtual bool doInitialization(CallGraph &CG) {
		Module &M = CG.getModule();

		//initializeDebugInfo(M);

		progress = 0;
		total = M.size();

		//cache_init();

		/*std::string line;
		std::ifstream fin(BLACKLIST);
		if (fin.is_open()) {
			while (std::getline(fin, line))
				blacklist.push_back(line);
			fin.close();
		}

		fin.open(SENSILIST);
		if (fin.is_open()) {
			while (std::getline(fin, line))
				sensilist.push_back(line);
			fin.close();
		}*/

		enter_atomic_context_functions.push_back("local_irq_save");

		return false;
	}

	virtual bool runOnSCC(CallGraphSCC &SCC) {
		for (auto node : SCC) {
			Function *F = node->getFunction();
			if (!F)
				continue;
			run_on_function(node, *F);
		}

		return false;
	}

	virtual bool doFinalization(CallGraph &CG) {
		//cache_finalize();
		return false;
	}

	virtual void print(raw_ostream &O, const Module *M) const {}
};

char RSC::ID = 0;

static RegisterPass<RSC> X("rsc", "RT-kernel Sleep-in-atomic Context",
			   false /* Only looks at CFG */,
			   false /* Analysis Pass */);
