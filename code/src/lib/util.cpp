#include "util.h"

#include <fstream>
#include <sstream>
#include <list>

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

using namespace llvm;

namespace rsc {

StringRef getFunctionName(Function *F) {
	return F->getName();
}

}; //end of namespace rsc