#include "FsigAnalysis.h"

#include <llvm/Support/raw_ostream.h>

#include "util.h"

using namespace llvm;

namespace rsc {

void FsigPointerAnalysis::copySig(llvm::Value *left, llvm::Value *right) {
	if (!left || !right)
		return;

	checkKnown(left);
	checkKnown(right);

	int left_score = sigs[left].score();
	int right_score = sigs[right].score();
	if (left_score > right_score) {
		sigs[right] = sigs[left];
		updated_sigs.push_back(right);
		done = false;
	} else if (left_score < right_score) {
		sigs[left] = sigs[right];
		updated_sigs.push_back(left);
		done = false;
	}
}

void FsigPointerAnalysis::composeSig(llvm::Value *left, const std::list<std::string> &comp,
				     int score, const char *separator,
				     const char *left_marker, const char *right_marker) {
	if (comp.empty())
		return;

	std::string buf;
	buf += left_marker;
	for (auto i = comp.begin(), e = comp.end(); i != e; i ++) {
		buf += *i;
		if (i != std::prev(e))
			buf += separator;
	}
	buf += right_marker;
	sigs[left].set_sig(buf);
	sigs[left].set_score(score);
	updated_sigs.push_back(left);
	done = false;
}

void FsigPointerAnalysis::checkKnown(llvm::Value *v) {
	if (!v || v == pseudo_return)
		return;

	if (sigs[v].score() < SCORE_CONSTANT) {
		if (auto ci = dyn_cast<ConstantInt>(v)) {
			scratch.clear();
			os << ci->getSExtValue();
			os.flush();
			sigs[v].set_sig(scratch);
			sigs[v].set_score(SCORE_CONSTANT);
		} else if (auto cpn = dyn_cast<ConstantPointerNull>(v)) {
			sigs[v].set_sig("0");
			sigs[v].set_score(SCORE_CONSTANT);
		}
	}
	if (sigs[v].score() < SCORE_GLOBAL_VAR && (dyn_cast<GlobalVariable>(v) != NULL)) {
		scratch.clear();
		os << "[" << v->getName() << "]";
		os.flush();
		sigs[v].set_sig(scratch);
		sigs[v].set_score(SCORE_GLOBAL_VAR);
	}
}

void FsigPointerAnalysis::composeGetElementPtrSig(GetElementPtrInst &I, std::list<std::string> &comp) {
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
				if (struct_type->getName().startswith("struct.")) {
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

bool FsigPointerAnalysis::handleSpecialFunction(CallInst &I) {
	Function *called = I.getCalledFunction();
	if (!called)
		return false;

	if (called->getName() == "__container_of") {
		Value *base = I.getArgOperand(0);
		checkKnown(base);
		if (sigs[base].score() <= sigs[&I].score())
			return true;
		Value *offset = I.getArgOperand(1);
		while (true) {
			if (auto ci = dyn_cast<CastInst>(offset)) {
				offset = ci->getOperand(0);
			} else {
				break;
			}
		}
		if (auto eei = dyn_cast<GetElementPtrInst>(offset)) {
			std::list<std::string> comp;
			composeGetElementPtrSig(*eei, comp);
			if (comp.empty())
				return false;
			sigs[&I] = sigs[base];
			scratch = sigs[&I].sig();
			for (auto i = comp.rbegin(), e = comp.rend(); i != e; ++i)
				os << ".-" << *i;
			os.flush();
			sigs[&I].set_sig(scratch);
			updated_sigs.push_back(&I);
			done = false;
		}
		return true;
	}

	return false;
}

FsigPointerAnalysis::FsigPointerAnalysis(Function &F, Context &c, path_iterator &path)
	: F(F), path(path), os(scratch), sigs(c)
{
	int count = 1;
	for (auto i = F.arg_begin(), e = F.arg_end(); i != e; i ++, count ++) {
		scratch.clear();
		os << "[" << count << "]";
		os.flush();
		sigs[i].set_sig(scratch);
		sigs[i].set_score(SCORE_FORMAL_PARAM);
	}

	sigs[pseudo_return].set_sig("[0]");
	sigs[pseudo_return].set_score(SCORE_RETURN_VALUE);

	revisit();
}

void FsigPointerAnalysis::visitLoadInst(LoadInst &I) {
	copySig(&I, I.getPointerOperand());
}

void FsigPointerAnalysis::visitStoreInst(StoreInst &I) {
	copySig(I.getPointerOperand(), I.getValueOperand());
}

void FsigPointerAnalysis::visitGetElementPtrInst(GetElementPtrInst &I) {
	std::list<std::string> comp;
	Value *op = I.getPointerOperand();
	int score = sigs[op].score();

	if (sigs[&I].score() < score) {
		composeGetElementPtrSig(I, comp);
		if (comp.empty())
			return;
		comp.push_front(sigs[op].sig());
	}

	composeSig(&I, comp, score, ".");
}

void FsigPointerAnalysis::visitCallInst(CallInst &I) {
	handleSpecialFunction(I);
}

void FsigPointerAnalysis::visitCastInst(CastInst &I) {
	copySig(&I, I.getOperand(0));
}

void FsigPointerAnalysis::visitPHINode(PHINode &I) {
	copySig(&I, path.determine_phinode(&I));
}

void FsigPointerAnalysis::visitReturnInst(ReturnInst &I) {
	Value *ret = I.getReturnValue();
	if (ret)
		copySig(ret, pseudo_return);
}

void FsigPointerAnalysis::add_constraint(llvm::Value *v, StringRef sig) {
	if (sigs[v].sig().equals(sig))
		return;

	bool is_number = atoi(sig.str()).first;

	sigs[v].set_sig(sig.str());
	if (is_number)
		sigs[v].set_score(SCORE_CONSTANT);
	else
		sigs[v].set_score(SCORE_FUNC_CALL);

	updated_sigs.push_back(v);
}

void FsigPointerAnalysis::revisit() {
	std::set<llvm::BasicBlock*> &bbs = *path;
	do {
		done = true;
		for (auto bb : bbs)
			visit(bb);
	} while (!done);
}

llvm::StringRef FsigPointerAnalysis::operator[](llvm::Value *v) {
	checkKnown(v);
	if (sigs[v].score() == SCORE_DEFAULT) {
		scratch.clear();
		os << "{" << v->getName() << "@" << F.getName() << "}";
		os.flush();
		sigs[v].set_sig(scratch);
	}
	return StringRef(sigs[v].sig());
}

void FsigPointerAnalysis::dump() {
	for (auto i : sigs) {
		if (i.second.score() == SCORE_DEFAULT)
			continue;

		scratch.clear();
		if (i.first == pseudo_return) {
			os << "returns";
		} else {
			i.first->printAsOperand(os, false);
		}
		os.flush();
		errs() << scratch << " ";
		errs() << (*this)[i.first] << "\n";
	}
	errs() << "\n";
}

};
