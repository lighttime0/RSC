#include "FsigAnalysis.h"

#include "llvm/Support/raw_ostream.h"

#include "util.h"

using namespace llvm;

namespace rid {

bool FsigConstantAnalysis::Range::is_constant() {
	switch(sig) {
	case SIG_NONNEGATIVE:
	case SIG_POSITIVE:
	case SIG_NONPOSITIVE:
	case SIG_NEGATIVE:
	case SIG_NONZERO:
	case SIG_ANY:
	case SIG_EMPTY:
		return false;
	}
	return true;
}

FsigConstantAnalysis::Range FsigConstantAnalysis::Range::intersects(Range r) {
	if (sig == SIG_ANY)
		return r;
	if (r.sig == SIG_ANY)
		return *this;
	if (sig == SIG_EMPTY || r.sig == SIG_EMPTY)
		return Range(SIG_EMPTY);

	switch (sig) {
	case SIG_NONNEGATIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_NONNEGATIVE);
		case SIG_POSITIVE:    return Range(SIG_POSITIVE);
		case SIG_NONPOSITIVE: return Range(0);
		case SIG_NEGATIVE:    return Range(SIG_EMPTY);
		case SIG_NONZERO:     return Range(SIG_POSITIVE);
		default:
			if (r.sig >= 0)
				return r.sig;
			return Range(SIG_EMPTY);
		}
	case SIG_POSITIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_POSITIVE);
		case SIG_POSITIVE:    return Range(SIG_POSITIVE);
		case SIG_NONPOSITIVE: return Range(SIG_EMPTY);
		case SIG_NEGATIVE:    return Range(SIG_EMPTY);
		case SIG_NONZERO:     return Range(SIG_POSITIVE);
		default:
			if (r.sig > 0)
				return r.sig;
			return Range(SIG_EMPTY);
		}
	case SIG_NONPOSITIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(0);
		case SIG_POSITIVE:    return Range(SIG_EMPTY);
		case SIG_NONPOSITIVE: return Range(SIG_NONPOSITIVE);
		case SIG_NEGATIVE:    return Range(SIG_NEGATIVE);
		case SIG_NONZERO:     return Range(SIG_NEGATIVE);
		default:
			if (r.sig <= 0)
				return r.sig;
			return Range(SIG_EMPTY);
		}
	case SIG_NEGATIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_EMPTY);
		case SIG_POSITIVE:    return Range(SIG_EMPTY);
		case SIG_NONPOSITIVE: return Range(SIG_NEGATIVE);
		case SIG_NEGATIVE:    return Range(SIG_NEGATIVE);
		case SIG_NONZERO:     return Range(SIG_NEGATIVE);
		default:
			if (r.sig < 0)
				return r.sig;
			return Range(SIG_EMPTY);
		}
	case SIG_NONZERO:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_POSITIVE);
		case SIG_POSITIVE:    return Range(SIG_POSITIVE);
		case SIG_NONPOSITIVE: return Range(SIG_NEGATIVE);
		case SIG_NEGATIVE:    return Range(SIG_NEGATIVE);
		case SIG_NONZERO:     return Range(SIG_NONZERO);
		default:
			if (r.sig != 0)
				return r.sig;
			return Range(SIG_EMPTY);
		}
	default:
		switch (r.sig) {
		case SIG_NONNEGATIVE:
			if (sig >= 0)
				return sig;
			return Range(SIG_EMPTY);
		case SIG_POSITIVE:
			if (sig > 0)
				return sig;
			return Range(SIG_EMPTY);
		case SIG_NONPOSITIVE:
			if (sig <= 0)
				return sig;
			return Range(SIG_EMPTY);
		case SIG_NEGATIVE:
			if (sig < 0)
				return sig;
			return Range(SIG_EMPTY);
		case SIG_NONZERO:
			if (sig != 0)
				return sig;
			return Range(SIG_EMPTY);
		default:
			if (sig == r.sig)
				return sig;
			return Range(SIG_EMPTY);
		}
	}

	return Range(SIG_EMPTY);
}

FsigConstantAnalysis::Range FsigConstantAnalysis::Range::unions(Range r) {
	if (sig == SIG_ANY || r.sig == SIG_ANY)
		return Range(SIG_ANY);
	if (sig == SIG_EMPTY)
		return r;
	if (r.sig == SIG_EMPTY)
		return *this;

	switch (sig) {
	case SIG_NONNEGATIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_NONNEGATIVE);
		case SIG_POSITIVE:    return Range(SIG_NONNEGATIVE);
		case SIG_NONPOSITIVE: return Range(SIG_ANY);
		case SIG_NEGATIVE:    return Range(SIG_ANY);
		case SIG_NONZERO:     return Range(SIG_ANY);
		default:
			if (r.sig >= 0)
				return r.sig;
			return Range(SIG_ANY);
		}
	case SIG_POSITIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_NONNEGATIVE);
		case SIG_POSITIVE:    return Range(SIG_POSITIVE);
		case SIG_NONPOSITIVE: return Range(SIG_ANY);
		case SIG_NEGATIVE:    return Range(SIG_NONZERO);
		case SIG_NONZERO:     return Range(SIG_NONZERO);
		default:
			if (r.sig > 0)
				return r.sig;
			return Range(SIG_ANY);
		}
	case SIG_NONPOSITIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_ANY);
		case SIG_POSITIVE:    return Range(SIG_ANY);
		case SIG_NONPOSITIVE: return Range(SIG_NONPOSITIVE);
		case SIG_NEGATIVE:    return Range(SIG_NONPOSITIVE);
		case SIG_NONZERO:     return Range(SIG_ANY);
		default:
			if (r.sig <= 0)
				return r.sig;
			return Range(SIG_ANY);
		}
	case SIG_NEGATIVE:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_ANY);
		case SIG_POSITIVE:    return Range(SIG_NONZERO);
		case SIG_NONPOSITIVE: return Range(SIG_NONPOSITIVE);
		case SIG_NEGATIVE:    return Range(SIG_NEGATIVE);
		case SIG_NONZERO:     return Range(SIG_NONZERO);
		default:
			if (r.sig < 0)
				return r.sig;
			return Range(SIG_ANY);
		}
	case SIG_NONZERO:
		switch (r.sig) {
		case SIG_NONNEGATIVE: return Range(SIG_ANY);
		case SIG_POSITIVE:    return Range(SIG_NONZERO);
		case SIG_NONPOSITIVE: return Range(SIG_ANY);
		case SIG_NEGATIVE:    return Range(SIG_NONZERO);
		case SIG_NONZERO:     return Range(SIG_NONZERO);
		default:
			if (r.sig != 0)
				return r.sig;
			return Range(SIG_ANY);
		}
	default:
		switch (r.sig) {
		case SIG_NONNEGATIVE:
			if (sig >= 0)
				return sig;
			return Range(SIG_ANY);
		case SIG_POSITIVE:
			if (sig > 0)
				return sig;
			return Range(SIG_ANY);
		case SIG_NONPOSITIVE:
			if (sig <= 0)
				return sig;
			return Range(SIG_ANY);
		case SIG_NEGATIVE:
			if (sig < 0)
				return sig;
			return Range(SIG_ANY);
		case SIG_NONZERO:
			if (sig != 0)
				return sig;
			return Range(SIG_ANY);
		default:
			if (sig == r.sig)
				return sig;
			/* This is too loose, but I hope we'll never reach this case... */
			return Range(SIG_ANY);
		}
	}

	return Range(SIG_ANY);
}

bool FsigConstantAnalysis::Range::subset_of(Range r) {
	if (*this == r)
		return true;
	if (intersects(r) == *this)
		return true;
	return false;
}

FsigConstantAnalysis::Range FsigConstantAnalysis::Range::negates() {
	switch(sig) {
	case SIG_NONNEGATIVE: return Range(SIG_NEGATIVE);
	case SIG_POSITIVE:    return Range(SIG_NONPOSITIVE);
	case SIG_NONPOSITIVE: return Range(SIG_POSITIVE);
	case SIG_NEGATIVE:    return Range(SIG_NONNEGATIVE);
	case SIG_NONZERO:     return Range(0);
	case SIG_ANY:         return Range(SIG_EMPTY);
	case SIG_EMPTY:       return Range(SIG_ANY);
	default:
		if (sig == 0)
			return Range(SIG_NONZERO);
		else if (sig > 0)
			return Range(SIG_NONPOSITIVE);
		else
			return Range(SIG_NONNEGATIVE);
	}
}

void FsigConstantAnalysis::setIntegerSignature(Value *v, int sig) {
	/* Try merging results from multiple predicates */
	Range &old = sigmap[v].range;
	Range n = old.intersects(Range(sig));

#if 0
	if (v != pseudo_return)
		v->printAsOperand(errs(), false);
	else
		errs() << "<return>";
	errs() << " ==> " << sig << "\n";
#endif

	if (old != n) {
		old = n;
		sigmap[v].score = SCORE_MAX;
		done = false;
	}
}

void FsigConstantAnalysis::copySig(llvm::Value *left, llvm::Value *right) {
	if (!left || !right)
		return;

	Range &l = sigmap[left].range, &r = sigmap[right].range;
	Range n = l.intersects(r);

#if 0
	if (left != pseudo_return)
		left->printAsOperand(errs(), false);
	else
		errs() << "<return>";
	errs() << "(" << getPrintedSignature(l) << ")";
	errs() << " <=> ";
	if (right != pseudo_return)
		right->printAsOperand(errs(), false);
	else
		errs() << "<return>";
	errs() << "(" << getPrintedSignature(r) << ")";
	errs() << "\n";
#endif

	if (n == Range::SIG_EMPTY) {
		_feasible = false;
		return;
	}

	if (l != n) {
		l = n;
		done = false;
		sigmap[left].score = SCORE_MAX;
	}
	if (r != n) {
		r = n;
		done = false;
		sigmap[right].score = SCORE_MAX;
	}
}

void FsigConstantAnalysis::inferEq(Value *v, Value *constant) {
	copySig(v, constant);
}

void FsigConstantAnalysis::inferNe(Value *v, Value *constant) {
	if (sigmap[constant].range.sig == 0)
		setIntegerSignature(v, Range::SIG_NONZERO);
}

void FsigConstantAnalysis::inferSlt(Value *v, Value *constant) {
	if (sigmap[constant].range.sig <= 0)
		setIntegerSignature(v, Range::SIG_NEGATIVE);
}

void FsigConstantAnalysis::inferSle(Value *v, Value *constant) {
	if (sigmap[constant].range.sig < 0)
		setIntegerSignature(v, Range::SIG_NEGATIVE);
	else if (sigmap[constant].range.sig == 0)
		setIntegerSignature(v, Range::SIG_NONPOSITIVE);
}

void FsigConstantAnalysis::inferSgt(Value *v, Value *constant) {
	if (sigmap[constant].range.sig >= 0)
		setIntegerSignature(v, Range::SIG_POSITIVE);
}

void FsigConstantAnalysis::inferSge(Value *v, Value *constant) {
	if (sigmap[constant].range.sig > 0)
		setIntegerSignature(v, Range::SIG_POSITIVE);
	else if (sigmap[constant].range.sig == 0)
		setIntegerSignature(v, Range::SIG_NONNEGATIVE);
}

void FsigConstantAnalysis::inferWithConstant(ICmpInst &I,
					     void (FsigConstantAnalysis::*infer)(Value*, Value*),
					     void (FsigConstantAnalysis::*infer_rev)(Value*, Value*)) {
	Value *left = I.getOperand(0), *right = I.getOperand(1);
	if (sigmap[right].range.is_constant())
		(this->*infer)(left, right);
	else if (sigmap[left].range.is_constant())
		(this->*infer_rev)(right, left);
}

void FsigConstantAnalysis::visitConstant(llvm::Constant *c) {
	if (auto ci = dyn_cast<ConstantInt>(c)) {
		setIntegerSignature(c, ci->getLimitedValue());
	} else if (auto cpn = dyn_cast<ConstantPointerNull>(c)) {
		setIntegerSignature(c, 0);
	} else if (auto ce = dyn_cast<ConstantExpr>(c)) {
		if (ce->isCast()) {
			Value *op = ce->getOperand(0);
			if (auto cc = dyn_cast<llvm::Constant>(op))
				visitConstant(cc);
			copySig(c, op);
		}
	}
}

FsigConstantAnalysis::FsigConstantAnalysis(Function &F, path_iterator &path)
	: F(F), path(path), _feasible(true)
{
	std::set<llvm::BasicBlock*> &bbs = *path;
	for_each_inst_in_bbs(bbs, inst) {
		for (int i = 0, e = inst->getNumOperands(); i != e; ++i) {
			Value *v = inst->getOperand(i);
			if (auto c = dyn_cast<llvm::Constant>(v))
				visitConstant(c);
		}
	}

	std::list<llvm::Value*> &true_preds = path.true_preds();
	std::list<llvm::Value*> &false_preds = path.false_preds();
	do {
		done = true;
		pred_holds = PRED_UNKNOWN;
		for (auto bb : bbs)
			visit(bb);
		pred_holds = PRED_HOLD;
		for (auto v : true_preds)
			if (auto inst = dyn_cast<Instruction>(v))
				visit(inst);
		pred_holds = PRED_NOT_HOLD;
		for (auto v : false_preds)
			if (auto inst = dyn_cast<Instruction>(v))
				visit(inst);
	} while (!done && _feasible);
}

void FsigConstantAnalysis::addPointerInfo(FsigPointerAnalysis &ptr_sigs) {
	this->ptr_sigs = &ptr_sigs;

	for (auto &p : sigmap) {
		llvm::Value *v = p.first;
		Signature &sig = p.second;

		llvm::StringRef ptr_sig = ptr_sigs[v];
		try {
			Signature &sig2 = sigmap_of_sig.at(ptr_sig);
			sig2.range = sig2.range.intersects(sig.range);
		} catch (std::out_of_range &e) {
			sigmap_of_sig[ptr_sig] = sig;
		}
	}

	try {
		llvm::StringRef ret_sig = ptr_sigs[pseudo_return];
		if (!ret_sig.equals("[0]")) {
			sigmap_of_sig["[0]"] = sigmap_of_sig[ret_sig];
		}
	} catch (std::out_of_range &e) {
	}
}

void FsigConstantAnalysis::visitLoadInst(LoadInst &I) {
	copySig(&I, I.getPointerOperand());
}

void FsigConstantAnalysis::visitStoreInst(StoreInst &I) {
	copySig(I.getPointerOperand(), I.getValueOperand());
}

void FsigConstantAnalysis::visitCastInst(CastInst &I) {
	if (auto c = dyn_cast<llvm::Constant>(I.getOperand(0)))
		visitConstant(c);
	copySig(&I, I.getOperand(0));
}

void FsigConstantAnalysis::visitICmpInst(llvm::ICmpInst &I) {
	bool branchTaken;
	if (pred_holds == PRED_HOLD)
		branchTaken = true;
	else if (pred_holds == PRED_NOT_HOLD)
		branchTaken = false;
	else
		return;

	void (FsigConstantAnalysis::*infer)(Value*, Value*) = NULL;
	void (FsigConstantAnalysis::*infer_rev)(Value*, Value*) = NULL;

	switch(I.getPredicate()) {
	case CmpInst::Predicate::ICMP_NE:
		branchTaken = !branchTaken;
	case CmpInst::Predicate::ICMP_EQ:
		if (branchTaken == true)
			infer = infer_rev = &FsigConstantAnalysis::inferEq;
		else
			infer = infer_rev = &FsigConstantAnalysis::inferNe;
		break;
	case CmpInst::Predicate::ICMP_SGE:
		branchTaken = !branchTaken;
	case CmpInst::Predicate::ICMP_SLT:
		if (branchTaken == true) {
			infer = &FsigConstantAnalysis::inferSlt;
			infer_rev = &FsigConstantAnalysis::inferSgt;
		} else {
			infer = &FsigConstantAnalysis::inferSge;
			infer_rev = &FsigConstantAnalysis::inferSle;
		}
		break;
	case CmpInst::Predicate::ICMP_SGT:
		branchTaken = !branchTaken;
	case CmpInst::Predicate::ICMP_SLE:
		if (branchTaken == true) {
			infer = &FsigConstantAnalysis::inferSle;
			infer_rev = &FsigConstantAnalysis::inferSge;
		} else {
			infer = &FsigConstantAnalysis::inferSgt;
			infer_rev = &FsigConstantAnalysis::inferSlt;
		}
		break;
	}
	if (infer && infer_rev)
		inferWithConstant(I, infer, infer_rev);
}

void FsigConstantAnalysis::visitCallInst(CallInst &I) {
	switch (pred_holds) {
	case PRED_HOLD:
		setIntegerSignature(&I, Range::SIG_NONZERO);
		break;
	case PRED_NOT_HOLD:
		setIntegerSignature(&I, 0);
		break;
	}
}

void FsigConstantAnalysis::visitPHINode(PHINode &I) {
	copySig(&I, path.determine_phinode(&I));
}

void FsigConstantAnalysis::visitReturnInst(ReturnInst &I) {
	Value *ret = I.getReturnValue();
	if (ret) {
		copySig(ret, pseudo_return);
	}
}

FsigConstantAnalysis::Range FsigConstantAnalysis::getSignature(Value *v) {
	if (sigmap[v].score == SCORE_DEFAULT)
		return Range(Range::SIG_ANY);
	return sigmap[v].range;
}

StringRef FsigConstantAnalysis::getPrintedSignature(Range r) {
	static std::string sigbuf;

	int sig = r.sig;
	switch (sig) {
	case Range::SIG_ANY:
		return StringRef("{?}");
	case Range::SIG_EMPTY:
		return StringRef("{}");
	case Range::SIG_NONZERO:
		return StringRef("{!0}");
	case Range::SIG_POSITIVE:
		return StringRef("{+}");
	case Range::SIG_NEGATIVE:
		return StringRef("{-}");
	case Range::SIG_NONPOSITIVE:
		return StringRef("{!+}");
	case Range::SIG_NONNEGATIVE:
		return StringRef("{!-}");
	default: {
		raw_string_ostream os(sigbuf);
		sigbuf.clear();
		os << "{" << sig << "}";
		os.flush();
 	}
	}
	return StringRef(sigbuf);
}

FsigConstantAnalysis::Range FsigConstantAnalysis::operator[](llvm::Value *v) {
	return getSignature(v);
}

FsigConstantAnalysis::Range FsigConstantAnalysis::operator[](llvm::StringRef sig) {
	if (sigmap_of_sig[sig].score == SCORE_DEFAULT)
		return Range(Range::SIG_ANY);
	return sigmap_of_sig[sig].range;
}

void FsigConstantAnalysis::dump() {
	std::string buf;
	raw_string_ostream os(buf);

	for (auto i : sigmap) {
		if (i.second.score == SCORE_DEFAULT)
			continue;

		buf.clear();
		if (i.first == pseudo_return) {
			os << "returns";
		} else {
			i.first->printAsOperand(os, false);
		}
		os.flush();
		errs() << buf << " ";
		errs() << getPrintedSignature(i.second.range) << "\n";
	}
	errs() << "\n";
}

std::map<llvm::StringRef, FsigConstantAnalysis::Signature>::iterator
FsigConstantAnalysis::begin() {
	return sigmap_of_sig.begin();
}

std::map<llvm::StringRef, FsigConstantAnalysis::Signature>::iterator
FsigConstantAnalysis::end() {
	return sigmap_of_sig.end();
}

};
