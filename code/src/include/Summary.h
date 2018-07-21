//===---- Summary.h - Function summaries. ----------------*- C++ -*-===//

#ifndef SUMMARY_H
#define SUMMARY_H

#include <list>
#include <map>
#include <string>
#include <fstream>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/regex.hpp>

#include "Formula.h"
#include "PathIterator.h"

namespace rsc {

typedef std::string VariableSignature;
typedef VariableSignature RefcountSig;
typedef VariableSignature RetValueSig;

struct SignatureData {
	VariableSignature sig;
	SignatureData() : sig("?") {}
};

class MultipathSignatureData {
	Context &c;
	std::map<int, SignatureData> sigs;

public:
	MultipathSignatureData(Context &c) : c(c) {}
	void init(const std::string &s) {
		sigs[0].sig = s;
	}
	llvm::StringRef get() {
		int id = c.pathid;
		while (id >= 0) {
			try {
				return sigs.at(id).sig;
			} catch (std::out_of_range &e) {}
			id = c.pathtree[id];
		}
		return sigs[c.pathid].sig;
	}
	void set(const std::string &s) {
		sigs[c.pathid].sig = s;
	}
	void replace(const std::string &o, const std::string &n) {
		try {
			auto &s = sigs.at(c.pathid);
			replace_all(s.sig, o, n);
		} catch (std::out_of_range &e) {}
	}
	void operator=(MultipathSignatureData &s) {
		set(s.get());
	}
	void print(llvm::raw_ostream &os) {
		for (auto &p : sigs)
			os << p.first << " -> " << p.second.sig << "\n";
	}
};

class SignatureMap : public std::map<llvm::Value*, MultipathSignatureData> {
	Context &c;
public:
	SignatureMap(Context &c) : c(c) {}
	MultipathSignatureData& operator[](llvm::Value *v);
};

struct Operation {
	int amount;

	// The following information are only used for reporting and debugging
	std::string refcount_sig;

	static unsigned int next_id;
	unsigned int id;
	std::list<Operation *> from;

	std::string host;

	Operation() : refcount_sig(""), amount(0), id(next_id++) {}

	void add_history_entry(Operation *op);

	void serialize(std::ofstream &fout);
	void deserialize(std::ifstream &fin);

	void print(llvm::raw_ostream &os, const std::string &prefix = "");
	void print_history(llvm::raw_ostream &os, const std::string &prefix = "");
};

class RefcountOps : public std::map<RefcountSig, Operation> {
public:
	virtual ~RefcountOps() {}

	bool diff(RefcountOps &rhs, std::set<std::pair<std::string, int>> &tainted);

	bool operator==(RefcountOps &rhs);
	bool operator!=(RefcountOps &rhs) { return !((*this) == rhs); }

	bool is_pure();

	void serialize(std::ofstream &fout);
	void deserialize(std::ifstream &fin);

	bool print(llvm::raw_ostream &os, const std::string &prefix = "", RefcountOps *otherpath = NULL);
};

struct PathSummaryEntry {
	Formula pc;
	RefcountOps ops;
	RetValueSig ret;

	/* Temporary fields used only during calculation */
	int id;
	bool applied;
	std::list<path_iterator::Edge*> path;
	Formula exact_pc;

	PathSummaryEntry() {}

	void serialize(std::ofstream &fout);
	void deserialize(std::ifstream &fin, Context &c);

	enum PathPrintType {
		NORMAL = 1,
		EXACT,
		BBPATH,
	};
	void print(llvm::raw_ostream &os, PathPrintType pt = NORMAL,
		   path_iterator *pi = NULL, PathSummaryEntry *otherpath = NULL,
		   const std::string &prefix = "\t", const std::string &delimiter = "\t");
};

typedef std::list<PathSummaryEntry> PathSummary;

class Summary {
	enum { UNKNOWN, PURE, IMPURE } pure;
public:
	llvm::Function *F;
	std::string name;
	Context c;
	std::list<PathSummaryEntry> summaries;
	std::list<PathSummaryEntry> dropped_summaries;

	/* Temoprary fields to control number of reports */
	std::set<std::pair<std::string, int>> tainted;

	Summary() : pure(UNKNOWN), F(NULL) {}

	typedef std::list<PathSummaryEntry>::iterator iterator;
	iterator begin() { return summaries.begin(); }
	iterator end() { return summaries.end(); }
	iterator erase(iterator i) { return summaries.erase(i); }

	bool is_pure();

	void serialize(std::ofstream &fout);
	void deserialize(std::ifstream &fin);

	bool print(llvm::raw_ostream &os, bool complete = false);
};

struct InstantiatedPathSummaryEntry {
	Formula pc;
	std::map<RefcountSig, Operation*> ops;

	RetValueSig ret;
};

class InstantiatedSummary {
	Context &c;

	static boost::regex container_elim_l;
	static boost::regex container_elim_r;

	static void formal_to_actual(VariableSignature &sig,
				     llvm::CallInst *ci,
				     SignatureMap &sigmap);

public:
	InstantiatedSummary(Summary &parameterized,
			    Context &c,
			    llvm::CallInst *ci,
			    SignatureMap &sigmap);

	std::list<InstantiatedPathSummaryEntry> summaries;

	std::list<InstantiatedPathSummaryEntry>::iterator begin() { return summaries.begin(); }
	std::list<InstantiatedPathSummaryEntry>::iterator end() { return summaries.end(); }

	bool print(llvm::raw_ostream &os);
};

typedef std::map<llvm::Function*, Summary> SummaryBase;

extern SummaryBase summaryBase;

bool has_predefined_summary(llvm::Function &F);
bool isPure(llvm::Function &F);
// void markImpure(llvm::Function &F);

void cache_open_fin(const std::string &name);
void cache_open_fout(const std::string &name);

void cache_init();
void cache_finalize();
bool serialize_summary(llvm::Function &F);
bool deserialize_summary(llvm::Function &F);

};

#endif /* SUMMARY_H */
