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

static cl::opt<std::string>
PREFIX("prefix",
       cl::init(""),
       cl::desc("Common prefix of sources"));

namespace rid {

std::string& replace_all(std::string& str, const std::string& old_value, const std::string& new_value)
{
	std::string::size_type pos(0);
	while ((pos = str.find(old_value, pos)) != std::string::npos) {
		str.replace(pos, old_value.length(), new_value);
		pos += new_value.length() - old_value.length() + 1;
	}
	return str;
}

bool starts_with(const std::string &str, const std::string &prefix) {
	return str.find(prefix) == 0;
}

bool ends_with(const std::string &str, const std::string &suffix) {
	return str.rfind(suffix) == str.size() - suffix.size();
}

std::pair<bool, long long> atoi(const std::string &str) {
	long long ret;
	std::stringstream s(str);
	s >> ret;

	if (!s.eof()) {
		return std::make_pair(false, 0);
	}
	return std::make_pair(true, ret);
}

StringRef print_type(Type *type) {
	static std::string buf;
	static raw_string_ostream os(buf);
	buf.clear();
	type->print(os);
	os.flush();
	replace_all(buf, ".", ":");
	replace_all(buf, "(", "<");
	replace_all(buf, ")", ">");
	replace_all(buf, " ", "");
	return StringRef(buf);
}

static std::set<std::string> fset;

void initializeInHeaderSet(Module &M) {
	if (!fset.empty())
		return;

	for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		bool done = false;
		for (auto bi = fi->begin(), be = fi->end(); bi != be && !done; ++bi) {
			for (auto ii = bi->begin(), ie = bi->end(); ii != ie && !done; ++ii) {
				DILocation dl(ii->getMetadata(LLVMContext::MD_dbg));
				if (dl.Verify() && ends_with(dl.getFilename(), ".h")) {
					fset.insert(fi->getName());
					done = true;
				}
			}
		}
	}
}

bool isHeaderStatic(llvm::Function *F) {
	return F->hasInternalLinkage() && fset.find(F->getName()) != fset.end();
}

bool isSourceStatic(llvm::Function *F) {
	return F->hasInternalLinkage() && fset.find(F->getName()) == fset.end();
}

StringRef getFunctionName(Function *F) {
	return F->getName();
}

std::map<std::string, std::vector<std::string> > fields;
std::map<llvm::MDNode*, bool> visited;
std::map<llvm::Function*, std::pair<std::string, std::string> > locations;

static void visitMDNode(MDNode *md) {
	DICompositeType desc(md);
	bool bitfield_added = false;

	if (desc.getTag() == dwarf::DW_TAG_structure_type) {
		std::string struct_name = desc.getName();
		DIArray elements = desc.getElements();
		for (int i = 0, e = elements.getNumElements(); i != e; ++i) {
			DIType element(static_cast<MDNode*>(elements.getElement(i)));
			if (element.getSizeInBits() < 8) {
				if (!bitfield_added) {
					fields[struct_name].push_back("<bitfield>");
					bitfield_added = true;
				}
			} else {
				fields[struct_name].push_back(element.getName());
				bitfield_added = false;
			}
		}
	} else if (desc.getTag() == dwarf::DW_TAG_subprogram) {
		DISubprogram subprogram(md);
		StringRef dir = desc.getDirectory(), file = desc.getFilename();
		if (subprogram.getFunction() && file.endswith(".h")) {
			fset.insert(subprogram.getFunction()->getName());
		}
		if (dir.startswith(PREFIX)) {
			if (dir.equals(PREFIX)) {
				locations[subprogram.getFunction()] =
					std::make_pair(subprogram.getName(), file);
			} else {
				locations[subprogram.getFunction()] =
					std::make_pair(subprogram.getName(),
						       (dir.substr(PREFIX.size()) + "/" + file).str());
			}
		} else {
			locations[subprogram.getFunction()] =
				std::make_pair(subprogram.getName(),
					       (dir + "/" + file).str());
		}
	}

	visited[md] = true;
	for (int i = 0, e = md->getNumOperands(); i != e; ++i)
		if (auto op = dyn_cast_or_null<MDNode>(md->getOperand(i)))
			if (!visited[op])
				visitMDNode(op);
}

void initializeDebugInfo(llvm::Module &M) {
	for (auto &nmd : M.getNamedMDList())
		for (int i = 0, e = nmd.getNumOperands(); i != e; ++i)
			visitMDNode(nmd.getOperand(i));
}

llvm::StringRef getField(const std::string &struct_name, int field_idx) {
	static std::string buf;
	static raw_string_ostream os(buf);

	if (fields.find(struct_name) == fields.end() ||
	    fields[struct_name].size() <= field_idx) {
		buf.clear();
		os << field_idx;
		os.flush();
		return buf;
	}

	return fields[struct_name][field_idx];
}

llvm::StringRef getInitialName(llvm::Function *F) {
	try {
		return locations.at(F).first;
	} catch (std::out_of_range &e) {
	}
	return F->getName();
}

llvm::StringRef getLocation(llvm::Function *F) {
	try {
		return locations.at(F).second;
	} catch (std::out_of_range &e) {
	}
	return "<unknown>";
}

void compose_get_element_ptr_sig(GetElementPtrInst &I, std::list<std::string> &comp) {
	std::string buf;
	raw_string_ostream os(buf);
	Value *op = I.getPointerOperand();
	Type *type = op->getType();

	for (auto i = I.idx_begin(), e = I.idx_end(); i != e; i ++) {
		Value *idx = i->get();
		long long index = -1;
		buf.clear();
		if (auto c = dyn_cast<ConstantInt>(idx))
			index = c->getSExtValue();
		if (type->isPointerTy() || type->isArrayTy() || type->isVectorTy()) {
			type = type->getSequentialElementType();
		} else if (type->isStructTy()) {
			StructType *struct_type = dyn_cast<StructType>(type);
			if (struct_type && index >= 0) {
				if (struct_type->isLiteral()) {
					os << index;
				} else if (struct_type->getName().startswith("struct.")) {
					os << getField(struct_type->getName().drop_front(7), index);
				} else if (struct_type->getName().startswith("union.")) {
					os << getField(struct_type->getName().drop_front(6), index);
				} else {
					os << index;
				}
			} else {
				os << "X";
			}
			type = type->getStructElementType(index);
			os.flush();
			comp.push_back(buf);
		} else {
			// Something we do not know. Skip it...
			comp.clear();
			return;
		}
	}
}

llvm::StringRef compose_sig(const std::list<std::string> &comp,
			    const char *separator,
			    const char *left_marker, const char *right_marker) {
	static std::string scratch;

	scratch.clear();
	if (comp.empty())
		return scratch;

	scratch += left_marker;
	for (auto i = comp.begin(), e = comp.end(); i != e; i ++) {
		scratch += *i;
		if (i != std::prev(e))
			scratch += separator;
	}
	scratch += right_marker;

	return scratch;
}

CmpInst::Predicate negate(CmpInst::Predicate p) {
	static CmpInst::Predicate negated_preds[] = {
		/* CmpInst::ICMP_EQ */ CmpInst::ICMP_NE,
		/* CmpInst::ICMP_NE */ CmpInst::ICMP_EQ,
		/* CmpInst::ICMP_UGT */ CmpInst::ICMP_ULE,
		/* CmpInst::ICMP_UGE */ CmpInst::ICMP_ULT,
		/* CmpInst::ICMP_ULT */ CmpInst::ICMP_UGE,
		/* CmpInst::ICMP_ULE */ CmpInst::ICMP_UGT,
		/* CmpInst::ICMP_SGT */ CmpInst::ICMP_SLE,
		/* CmpInst::ICMP_SGE */ CmpInst::ICMP_SLT,
		/* CmpInst::ICMP_SLT */ CmpInst::ICMP_SGE,
		/* CmpInst::ICMP_SLE */ CmpInst::ICMP_SGT,
	};

	if (p < CmpInst::FIRST_ICMP_PREDICATE || p > CmpInst::LAST_ICMP_PREDICATE)
		return CmpInst::BAD_ICMP_PREDICATE;
	return negated_preds[p - CmpInst::FIRST_ICMP_PREDICATE];
}

CmpInst::Predicate commute(CmpInst::Predicate p) {
	static CmpInst::Predicate commuted_preds[] = {
		/* CmpInst::ICMP_EQ */ CmpInst::ICMP_EQ,
		/* CmpInst::ICMP_NE */ CmpInst::ICMP_NE,
		/* CmpInst::ICMP_UGT */ CmpInst::ICMP_ULT,
		/* CmpInst::ICMP_UGE */ CmpInst::ICMP_ULE,
		/* CmpInst::ICMP_ULT */ CmpInst::ICMP_UGT,
		/* CmpInst::ICMP_ULE */ CmpInst::ICMP_UGE,
		/* CmpInst::ICMP_SGT */ CmpInst::ICMP_SLT,
		/* CmpInst::ICMP_SGE */ CmpInst::ICMP_SLE,
		/* CmpInst::ICMP_SLT */ CmpInst::ICMP_SGT,
		/* CmpInst::ICMP_SLE */ CmpInst::ICMP_SGE,
	};

	if (p < CmpInst::FIRST_ICMP_PREDICATE || p > CmpInst::LAST_ICMP_PREDICATE)
		return CmpInst::BAD_ICMP_PREDICATE;
	return commuted_preds[p - CmpInst::FIRST_ICMP_PREDICATE];
}

};
