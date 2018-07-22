#include <iostream>
#include <cstring>
#include <list>
#include <algorithm>

#include <boost/regex.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include "llvm/IR/InstIterator.h"

using namespace llvm;
//using namespace llvm::PatternMatch;

//namespace {

class MaySleeping : public FunctionPass {

private:

	std::list<std::string> may_sleeping_primitive;

public:
	static char ID;
	MaySleeping() : FunctionPass(ID) {}

	virtual bool doInitialization(Function &F) {
		may_sleeping_primitive.push_back("spin_lock_irqsave");
		return false;
	}

	virtual bool runOnFunction(Function &F) {
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
			if (auto *CallInst = dyn_cast<CallInst>(&I)) {
				StringRef cnt_fn = getFunctionName(CallInst->getCalledFunction());
				cnt_fn_name_str = cnt_fn.str();
				std::list<std::string>::iterator it = find(may_sleeping_primitive.begin(), 
									may_sleeping_primitive.end(), cnt_fn_name_str)
				if (it != may_sleeping_primitive.end()) {
					std::cout << cnt_fn_name_str << std::endl
				}
			}
		}
		return false;
	}
};



/*	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	Value *createOverflowBit(Intrinsic::ID ID, Value *L, Value *R) {
		Module *M = Builder->GetInsertBlock()->getParent()->getParent();
		Function *F = Intrinsic::getDeclaration(M, ID, L->getType());
		CallInst *CI = Builder->CreateCall2(F, L, R);
		return Builder->CreateExtractValue(CI, 1);
	}

	Value *matchCmpWithSwappedAndInverse(CmpInst::Predicate Pred, Value *L, Value *R) {
		// Try match comparision and the swapped form.
		if (Value *V = matchCmpWithInverse(Pred, L, R))
			return V;
		Pred = CmpInst::getSwappedPredicate(Pred);
		if (Value *V = matchCmpWithInverse(Pred, R, L))
			return V;
		return NULL;
	}

	Value *matchCmpWithInverse(CmpInst::Predicate Pred, Value *L, Value *R) {
		// Try match comparision and the inverse form.
		if (Value *V = matchCmp(Pred, L, R))
			return V;
		Pred = CmpInst::getInversePredicate(Pred);
		if (Value *V = matchCmp(Pred, L, R))
			return Builder->CreateXor(V, 1);
		return NULL;
	}

	Value *matchCmp(CmpInst::Predicate, Value *, Value *);

	bool removeRedundantZeroCheck(BasicBlock *);
};

} // anonymous namespace
*/

/*
bool MaySleeping::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		ICmpInst *I = dyn_cast<ICmpInst>(&*i);
		++i;
		if (!I)
			continue;
		Value *L = I->getOperand(0), *R = I->getOperand(1);
		if (!L->getType()->isIntegerTy())
			continue;
		Builder->SetInsertPoint(I);
		Value *V = matchCmpWithSwappedAndInverse(I->getPredicate(), L, R);
		if (!V)
			continue;
		I->replaceAllUsesWith(V);
		if (I->hasName())
			V->takeName(I);
		RecursivelyDeleteTriviallyDeadInstructions(I);
		Changed = true;
	}
	return Changed;
}
*/

/*
Value *MaySleeping::matchCmp(CmpInst::Predicate Pred, Value *L, Value *R) {
	Value *X, *Y, *A, *B;
	ConstantInt *C;

	// x != (x * y) /u y => umul.overflow(x, y)
	// x != (y * x) /u y => umul.overflow(x, y)
	if (Pred == CmpInst::ICMP_NE
	    && match(L, m_Value(X))
	    && match(R, m_UDiv(m_Mul(m_Value(A), m_Value(B)), m_Value(Y)))
	    && ((A == X && B == Y) || (A == Y && B == X)))
		return createOverflowBit(Intrinsic::umul_with_overflow, X, Y);

	// x >u C /u y =>
	//    umul.overflow(x, y),                       if C == UMAX
	//    x >u C || y >u C || umul.overflow_k(x, y), if C == UMAX_k
	//    (zext x) * (zext y) > (zext N),            otherwise.
	if (Pred == CmpInst::ICMP_UGT
	    && match(L, m_Value(X))
	    && match(R, m_UDiv(m_ConstantInt(C), m_Value(Y)))) {
		const APInt &Val = C->getValue();
		if (Val.isMaxValue())
			return createOverflowBit(Intrinsic::umul_with_overflow, X, Y);
		if ((Val + 1).isPowerOf2()) {
			unsigned N = Val.countTrailingOnes();
			IntegerType *T = IntegerType::get(Builder->getContext(), N);
			return Builder->CreateOr(
				Builder->CreateOr(
					Builder->CreateICmpUGT(X, C),
					Builder->CreateICmpUGT(Y, C)
				),
				createOverflowBit(Intrinsic::umul_with_overflow,
					Builder->CreateTrunc(X, T),
					Builder->CreateTrunc(Y, T)
				)
			);
		}
		unsigned N = X->getType()->getIntegerBitWidth() * 2;
		IntegerType *T = IntegerType::get(Builder->getContext(), N);
		return Builder->CreateICmpUGT(
			Builder->CreateMul(
				Builder->CreateZExt(X, T),
				Builder->CreateZExt(Y, T)
			),
			Builder->CreateZExt(C, T)
		);
	}

	return NULL;
}
*/

char MaySleeping::ID = 0;

static RegisterPass<MaySleeping> X("may-sleeping", "Finding functions that may sleeping",
			   false /* Only looks at CFG */,
			   false /* Analysis Pass */);
