//===---- util.h ------------------------------------------------*- C++ -*-===//

#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <set>
#include <list>
#include <algorithm>

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

namespace rsc {

llvm::StringRef getFunctionName(llvm::Function *F);

}; //end of namespace rsc

#endif /** UTIL_H **/