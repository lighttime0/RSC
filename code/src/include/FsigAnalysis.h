//===---- FsigAnalysis.h ----------------------------------------*- C++ -*-===//

#ifndef FSIG_ANALYSIS_H
#define FSIG_ANALYSIS_H

#include <cstdio>
#include <set>
#include <list>
#include <map>
#include <string>

#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"

#include "PathIterator.h"
#include "Formula.h"

namespace rsc {

static llvm::Value *pseudo_return = (llvm::Value *)(0xdead);

class FsigPointerAnalysis : public llvm::InstVisitor<FsigPointerAnalysis> {
	llvm::Function &F;
	path_iterator &path;

	bool done;
	std::list<path_iterator::Edge*> edges;
	path_iterator::Edge *current_in_edge;
	llvm::Value *retval;

	/*
	 * Each signature in the map is marked with a score ranging from 0 to
	 * 100. Signatures with higher scores may overwrite those with lower
	 * scores when building signatures.
	 */
	struct SignatureData {
		std::string sig;
		int score;

		SignatureData() : score(SCORE_DEFAULT), sig("?") {}
	};
	class Signature {
		Context &c;
		SignatureData base;
		std::map<int, SignatureData> extra;

	public:
		Signature(Context &c) : c(c) {}

		int score() {
			if (c.pathid >= 0)
				try {
					return extra.at(c.pathid).score;
				} catch (std::out_of_range &e) {}
			return base.score;
		}
		llvm::StringRef sig() {
			if (c.pathid >= 0)
				try {
					return extra.at(c.pathid).sig;
				} catch (std::out_of_range &e) {}
			return llvm::StringRef(base.sig);
		}
		void set_score(int s) {
			if (c.pathid >= 0)
				extra[c.pathid].score = s;
			else
				base.score = s;
		}
		void set_sig(const std::string &s) {
			if (c.pathid >= 0)
				extra[c.pathid].sig = s;
			else
				base.sig = s;
		}

		void operator=(Signature &s) {
			if (c.pathid >= 0) {
				extra[c.pathid].score = s.extra[c.pathid].score;
				extra[c.pathid].sig = s.extra[c.pathid].sig;
			} else {
				base.score = s.base.score;
				base.sig = s.base.sig;
			}
		}
	};

	class SignatureMap : public std::map<llvm::Value*, std::map<path_iterator::Edge*, Signature>> {
		Context &c;
	public:
		SignatureMap(Context &c) : c(c) {}
		Signature& operator[](llvm::Value *v) {
			try {
				return this->at(v);
			} catch (std::out_of_range &e) {}
			return (*((this->insert(std::make_pair(v,Signature(c)))).first)).second;
		}
	};

	SignatureMap sigs;
	std::list<std::pair<llvm::Value*, path_iterator::Edge*>> updated_sigs;

	class iterator {
		std::map<llvm::Value*, Signature>::iterator it;

	public:
		iterator(std::map<llvm::Value*, Signature>::iterator it) : it(it) {}

		iterator& operator++() {
			++it;
			return *this;
		}

		llvm::Value* operator*() { return it->first; }

		bool operator==(const iterator &rhs) const { return it == rhs.it; }
		bool operator!=(const iterator &rhs) const { return it != rhs.it; }
	};

	std::string scratch;
	llvm::raw_string_ostream os;

	static const int SCORE_MIN           =   0;
	static const int SCORE_MAX           = 100;
	static const int SCORE_FUNC_CALL     =  50;
	static const int SCORE_RETURN_VALUE  =  80;
	static const int SCORE_FORMAL_PARAM  =  90;
	static const int SCORE_CONSTANT      =  95;
	static const int SCORE_DEFAULT       =  SCORE_MIN;
	static const int SCORE_GLOBAL_VAR    =  SCORE_FORMAL_PARAM;

	Signature *get_sig_backward(llvm::Value *v);
	Signature &get_sig_here(llvm::Value *v);
	Signature *get_sig_forward(llvm::Value *v);

	void copy_sig(llvm::Value *left, llvm::Value *right);
	void compose_sig(llvm::Value *left, const std::list<std::string> &comp,
			int score, const char *separator,
			const char *left_marker = "", const char *right_marker = "");
	void check_known(llvm::Value *v);
	void add_updated(llvm::Value *v);

	void composeGetElementPtrSig(llvm::GetElementPtrInst &I, std::list<std::string> &comp);

	bool handleSpecialFunction(llvm::CallInst &I);

public:
	FsigPointerAnalysis(llvm::Function &F, Context &c, path_iterator &path);

	void visitLoadInst(llvm::LoadInst &I);
	void visitStoreInst(llvm::StoreInst &I);
	void visitGetElementPtrInst(llvm::GetElementPtrInst &I);
	void visitCallInst(llvm::CallInst &I);
	void visitCastInst(llvm::CastInst &I);
	void visitPHINode(llvm::PHINode &I);
	void visitReturnInst(llvm::ReturnInst &I);

	void add_constraint(llvm::Value *v, path_iterator::Edge *in_edge, llvm::StringRef sig);
	void revisit();
	std::list<llvm::Value*> &updated() { return updated_sigs; }
	void forget_updated() { updated_sigs.clear(); }

	iterator begin() { return iterator(sigs.begin()); }
	iterator end() { return iterator(sigs.end()); }

	llvm::StringRef operator[](llvm::Value *v);
	llvm::StringRef get_retsig() {
		if (retval)
			return get_sig(retval);
		return "";
	}
	llvm::StringRef get_sig(llvm::Value *v, path_iterator::Edge *in_edge = NULL);
	void dump();
};

};

#endif  // FSIG_ANALYSIS_H
