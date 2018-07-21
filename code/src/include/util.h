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

std::string& replace_all(std::string& str, const std::string& old_value, const std::string& new_value);
bool starts_with(const std::string &str, const std::string &prefix);
bool ends_with(const std::string &str, const std::string &suffix);
std::pair<bool, long long> atoi(const std::string &str);
llvm::StringRef print_type(llvm::Type *type);

typedef std::set<llvm::Function *> FunctionSet;

void initializeInHeaderSet(llvm::Module &M);
void initializeDebugInfo(llvm::Module &M);

bool isHeaderStatic(llvm::Function *F);
bool isSourceStatic(llvm::Function *F);
llvm::StringRef getFunctionName(llvm::Function *F);
llvm::StringRef getField(const std::string &struct_name, int field_idx);
llvm::StringRef getInitialName(llvm::Function *F);
llvm::StringRef getLocation(llvm::Function *F);

void compose_get_element_ptr_sig(llvm::GetElementPtrInst &I, std::list<std::string> &comp);
llvm::StringRef compose_sig(const std::list<std::string> &comp,
			    const char *separator,
			    const char *left_marker = "", const char *right_marker = "");

llvm::CmpInst::Predicate negate(llvm::CmpInst::Predicate p);
llvm::CmpInst::Predicate commute(llvm::CmpInst::Predicate p);

#define gfind(container, val)			\
	(std::find((container).begin(), (container).end(), (val)))
#define grfind(container, val)			\
	(std::find((container).rbegin(), (container).rend(), (val)))
#define gexist(container, val)			\
	(gfind(container, val) != (container).end())
#define gerase(container, val)						\
	(container).erase(gfind(container, val))
#define for_each_bb(F, bb)						\
	for (auto bb = F.begin(), _bbe = F.end(); bb != _bbe; ++bb)
#define for_each_inst(F, inst)						\
	for (auto _bb = (F).begin(), _bbe = (F).end(); _bb != _bbe; ++_bb) \
		for (auto inst = _bb->begin(), __ie = _bb->end(); inst != __ie; ++inst)
#define for_each_inst_in_bb(bb, inst)					\
	for (auto inst = (bb)->begin(), __ie = (bb)->end(); inst != __ie; ++inst)
#define for_each_inst_in_bbs(F, inst)						\
	for (auto _bb = F.begin(), _bbe = F.end(); _bb != _bbe; ++_bb)	\
		for (auto inst = (*_bb)->begin(), __ie = (*_bb)->end(); inst != __ie; ++inst)
#define for_each_bit(bv, bit)			\
	for (auto bit = bv.find_first(); bit >= 0; bit = bv.find_next(bit))

};

#endif /** UTIL_H **/
