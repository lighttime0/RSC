//===---- PathIterator.h - CFG Simple Path Iter. ----------------*- C++ -*-===//

#ifndef PATHITERATOR_H
#define PATHITERATOR_H

#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>

#include "llvm/ADT/BitVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"

#include "util.h"
#include "Formula.h"

namespace rsc {

class path_iterator {
public:
	struct Edge;
	struct Vertex {
		int id;
		std::string name;
		llvm::BasicBlock *bb;
		std::vector<Edge*> in_edges;
		std::vector<Edge*> out_edges;

		bool in_slice;

		// Whether it is safe to force including this Vertex in paths
		bool safe_to_include;
		llvm::BitVector directly_reachable;
		llvm::BitVector indirectly_reachable;

		int _start, _end;  // Temporary variables for DFS
		int _count;        // A temporary variable used in counting paths
	};

	struct Edge {
		int id;
		struct Vertex *source, *target;

		rid::Formula cond;

		llvm::BitVector dominated_edges;
		std::map<llvm::PHINode*, llvm::BitVector> phi_values;
		bool self_loop;
	};

private:

	rid::Context &c;
	llvm::Function &F;

	std::vector<Vertex*> vertices;
	Vertex *exit;
	llvm::ReturnInst *ret_inst;
	std::vector<Edge*> edges;
	int nr_original_edges;
	int branches;
	std::map<llvm::BasicBlock*, int> bbmap;

	std::list<Edge*> path;
	llvm::BitVector pathbv;
	llvm::BitVector path_dominated_edges;
	std::set<llvm::BasicBlock*> bbpath;

	int count_before_slicing;
	int count_after_slicing;

	path_iterator(llvm::Function &F, rid::Context &c) : F(F), c(c) {}
	path_iterator(llvm::Function &F, rid::Context &c, bool dummy);

	Edge *add_edge(Vertex *from, Vertex *to);
	void connect_edge(Edge *e1, Edge *e2);

	void add_to_path(Edge *edge);
	Edge *remove_from_path();
	bool can_add(Edge *edge);
	bool switch_edge();
	bool fill_path();
	bool is_feasible_path();

	void slice();
	void mark_inclusion_safety();
	void reduce_paths();

	void dfs();
	void count_recursive(Vertex *v, llvm::BitVector &bv);
	int count_internal();

public:
	virtual ~path_iterator();

	static path_iterator begin(llvm::Function &F, rid::Context &c);
	static path_iterator end(llvm::Function &F, rid::Context &c);

	path_iterator &operator++();
	bool operator!=(const path_iterator &x) const;
	std::set<llvm::BasicBlock*> &operator*();
	Edge *get_edge(int id) { return edges[id]; }
	std::list<Edge*> &get_path();
	llvm::BasicBlock *get_entry();
	rid::Formula path_condition();
	llvm::Value *determine_phinode(llvm::PHINode *phi);
	llvm::Value *determine_phinode(llvm::PHINode *phi, Edge *edge);

	int count();
	int count_sliced();
	int nr_branches();

	void pretty_print_path(llvm::raw_ostream &os, std::list<Edge*> &p);
	void dump_to_dot(const std::string &filename);
	void plot_path_pair(const std::string &filename, std::list<Edge*> &p1, std::list<Edge*> &p2);
};

static path_iterator path_begin(llvm::Function &F, rid::Context &c) {
	return path_iterator::begin(F, c);
}

static path_iterator path_end(llvm::Function &F, rid::Context &c) {
	return path_iterator::end(F, c);
}

};

#endif /* PATHITERATOR_H */
