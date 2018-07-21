#include "PathIterator.h"

#include <climits>
#include <fstream>
#include <string>
#include <algorithm>

#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/CommandLine.h>

#include "util.h"
#include "Summary.h"
#include "FormulaVisitor.h"

using namespace llvm;

static cl::opt<bool>
ForceExclude("force-exclude",
	     cl::init(true),
	     cl::desc("Force excluding 'safe' basic blocks"));

namespace rsc {

static Type *getUnderlyingType(Value *v) {
	Type *t = v->getType();
	while (auto st = dyn_cast<SequentialType>(t))
		t = t->getSequentialElementType();
	return t;
}

path_iterator::path_iterator(Function &F, Context &c, bool dummy)
	: F(F), c(c), nr_original_edges(0), exit(0), branches(0) {
	unsigned int vnum = 0;

	// Build Vertex Info
	for_each_bb(F, bb) {
		Vertex *v = new Vertex;
		v->id = vnum;
		v->bb = bb;
		vertices.push_back(v);
		bbmap[bb] = vnum;
		vnum++;

		for (auto &inst : *bb) {
			if (auto ri = dyn_cast<ReturnInst>(&inst)) {
				exit = v;
				ret_inst = ri;
			}
		}
	}
	for_each_bb(F, bb) {
		Vertex *v = vertices[bbmap[bb]];
		for (succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se; ++si) {
			Vertex *succ_v = vertices[bbmap[*si]];
			add_edge(v, succ_v);
		}
	}
	nr_original_edges = edges.size();

	// Generate Vertex names based on the BB
	for (auto v : vertices) {
		raw_string_ostream os(v->name);
		os << v->id << ":" << " \\\"";
		v->bb->printAsOperand(os);
		os << "\\\"";
		os.flush();
	}

	// assert(exit != NULL);
	count_before_slicing = count_internal();

	// Build predicate info
	for_each_bb(F, bb) {
		if (auto branch = dyn_cast<BranchInst>(&bb->back())) {
			if (branch->isUnconditional())
				continue;
			assert(branch->getNumSuccessors() == 2);
			llvm::Value *pred = branch->getCondition();
			Formula cond = c.get_atom(pred);
			auto v = vertices[bbmap[bb]];
#define ADD_PREDICATE(succ, b)						\
			do {						\
				auto target_v = vertices[bbmap[succ]];	\
				for (auto e : v->out_edges) {		\
					if (e->target == target_v) {	\
						if (b)			\
							e->cond = cond;	\
						else			\
							e->cond = !cond; \
						break;			\
					}				\
				}					\
			} while (0)

			ADD_PREDICATE(branch->getSuccessor(0), true);
			ADD_PREDICATE(branch->getSuccessor(1), false);
#undef ADD_PREDICATE
			branches ++;
		}
	}
	for (auto e : edges) {
		e->dominated_edges.resize(nr_original_edges);
		e->dominated_edges.set(e->id);
	}

	// phi-node info
	for_each_inst(F, inst) {
		auto phi = dyn_cast<PHINode>(inst);
		if (!phi)
			continue;
		BasicBlock *parent = phi->getParent();
		Vertex *v = vertices[bbmap[parent]];
		for (int i = 0, e = phi->getNumIncomingValues(); i < e; ++i) {
			int idx = i;
			Value *val = phi->getIncomingValue(i);
			BasicBlock *bb = phi->getIncomingBlock(i);
			for (int j = 0; j < idx; ++j) {
				if (val != phi->getIncomingValue(j))
					continue;
				idx = j;
				break;
			}
			for(auto edge : v->in_edges) {
				if (edge->source->bb != bb)
					continue;
				edge->phi_values[phi].resize(e);
				edge->phi_values[phi].set(idx);
				break;
			}
		}
	}

	for (auto v : vertices) {
		v->directly_reachable.resize(vertices.size());
		v->indirectly_reachable.resize(vertices.size());
	}

	// mark_inclusion_safety();
	// slice();
	// reduce_paths();

	count_after_slicing = count_internal();

	// Create the first path
	// Fallback to path-nonsensitive version if no exit block is found
	if (exit) {
		pathbv.resize(edges.size());
		path_dominated_edges.resize(nr_original_edges);
		if (!exit->in_edges.empty()) {
			add_to_path(exit->in_edges.front());
			fill_path();
			if (!is_feasible_path())
				++(*this);
		} else if (vertices.size() == 1) {
			path.push_front(NULL);
		}
	}
}

path_iterator::~path_iterator() {
	for (auto i : vertices)
		delete i;
	for (auto i : edges)
		delete i;
}

path_iterator::Edge *path_iterator::add_edge(Vertex *from, Vertex *to) {
	Edge *e = new Edge;

	e->id = edges.size();
	e->source = from;
	e->target = to;
	e->dominated_edges.resize(nr_original_edges);
	e->cond = True::get(c);
	e->self_loop = (from == to);

	from->out_edges.push_back(e);
	to->in_edges.push_back(e);

	edges.push_back(e);
	return e;
}

void path_iterator::connect_edge(Edge *e1, Edge *e2) {
	assert(e1->target == e2->source);
	Vertex *source = e1->source, *target = e2->target;
	Edge *new_edge = NULL;

	for (auto e : source->out_edges) {
		if (e->target == target) {
			bool consistent = true;
			for (auto &p : e->phi_values) {
				PHINode *phi = p.first;
				BitVector &bv = p.second;
				try {
					if (bv != e1->phi_values.at(phi)) {
						consistent = false;
						break;
					}
				} catch (std::out_of_range &e) {}
				try {
					if (bv != e2->phi_values.at(phi)) {
						consistent = false;
						break;
					}
				} catch (std::out_of_range &e) {}
			}
			if (consistent) {
				new_edge = e;
				break;
			}
		}
	}

	if (new_edge) {
		llvm::BitVector tmp = e1->dominated_edges;
		tmp |= e2->dominated_edges;
		new_edge->dominated_edges &= tmp;
		new_edge->cond = (new_edge->cond || (e1->cond && e2->cond))->deep_simplify();
	} else {
		new_edge = add_edge(source, target);
		new_edge->dominated_edges = e1->dominated_edges;
		new_edge->dominated_edges |= e2->dominated_edges;
		new_edge->cond = (e1->cond && e2->cond);
	}

	for (auto &p : e1->phi_values) {
		new_edge->phi_values[p.first].resize(p.second.size());
		new_edge->phi_values[p.first] |= p.second;
	}
	for (auto &p : e2->phi_values) {
		new_edge->phi_values[p.first].resize(p.second.size());
		new_edge->phi_values[p.first] |= p.second;
	}
}

void path_iterator::add_to_path(Edge *edge) {
	path.push_front(edge);
	pathbv.set(edge->id);
}

path_iterator::Edge *path_iterator::remove_from_path() {
	Edge *edge = path.front();
	path.pop_front();
	pathbv.reset(edge->id);
	return edge;
}

bool path_iterator::can_add(Edge *edge) {
	if (pathbv.test(edge->id))
		return false;
	if (edge->self_loop)
		return false;
	return true;
}

bool path_iterator::switch_edge() {
	Edge *edge;
	std::vector<Edge*>::iterator i;

	do {
		if (path.empty())
			return false;

		edge = remove_from_path();
		i = gfind(edge->target->in_edges, edge);
		do {
			++i;
		} while (i != edge->target->in_edges.end() && !can_add(*i));
	} while (i == edge->target->in_edges.end());

	add_to_path(*i);
	return true;
}

bool path_iterator::fill_path() {
	Vertex *entry = vertices.front();
	while (path.front()->source != entry) {
		Edge *edge = path.front();
		bool found = false;
		for (auto e : edge->source->in_edges) {
			if (!can_add(e))
				continue;
			add_to_path(e);
			found = true;
			break;
		}
		if (found)
			continue;
		if (!switch_edge())
			return false;
	}

	path_dominated_edges.clear();
	for (auto e : path) {
		path_dominated_edges |= e->dominated_edges;
	}

	return true;
}

bool path_iterator::is_feasible_path() {
	return true;
}

void path_iterator::slice() {
	/* 1. Initialize the graph */
	for (auto v : vertices)
		v->in_slice = false;

	/* 2. Determine the slice criteria */
	std::list<Value*> criteria;
	criteria.push_back(ret_inst);
	for_each_inst(F, inst) {
		if (auto phi = dyn_cast<PHINode>(inst))
			criteria.push_back(phi);
		CallInst *ci = dyn_cast<CallInst>(inst);
		if (!ci)
			continue;
		Function *f = ci->getCalledFunction();
		if (!f)
			continue;
		if (isPure(*f))
			continue;
		criteria.push_back(ci);
	}

	/* 3. Enumerate backward dependencies... */
	std::set<Value *> visited;
	for (auto i : criteria)
		visited.insert(i);
	while (!criteria.empty()) {
		Value *v = criteria.front();
		criteria.pop_front();
		if (!v)	continue;
		Instruction *inst = dyn_cast<Instruction>(v);
		if (!inst) continue;

		vertices[bbmap[inst->getParent()]]->in_slice = true;

		for (auto i = inst->value_op_begin(), e = inst->value_op_end(); i != e; ++i)
			if (visited.insert(*i).second)
				criteria.push_back(*i);

		/* If the value is used in predicates, we should preserve these
		 * bbs so that the predicates can be precisely tracked
		 *
		 * UPDATE: These predicates are used in the range analysis which
		 * is deprecated since the introduction of SMT.
		 */
		// for (auto i = inst->user_begin(), e = inst->user_end(); i != e; ++i)
		// 	if (auto ci = dyn_cast<CmpInst>(*i))
		// 		vertices[bbmap[ci->getParent()]]->in_slice = true;

		if (auto ri = dyn_cast<ReturnInst>(inst)) {
			Value *next = ri->getReturnValue();
			if (visited.insert(next).second)
				criteria.push_back(next);
		} else if (auto pn = dyn_cast<PHINode>(inst)) {
			for (int i = 0, e = pn->getNumIncomingValues(); i != e; ++i) {
				Value *incoming = pn->getIncomingValue(i);
				if (!dyn_cast<ConstantInt>(incoming) && visited.insert(incoming).second) {
					criteria.push_back(incoming);
				}
			}
		}
	}

	/* Analysis on impacts of return values (of functions w/ refcount
	 * operations) are no longer needed. The main purpose for this analysis
	 * is to preserve the predicates related to the return values so that we
	 * can build precise constraints in the range analysis. As we have
	 * deprecate the range analysis and switch to SMT formulas, the impact
	 * analysis can be safely ignored.
	 */

	for (auto v : vertices)
		v->safe_to_include = !v->in_slice;
}

void path_iterator::mark_inclusion_safety() {
	for (auto v : vertices) {
		bool safe = true;
		if (v->in_edges.empty() || v->out_edges.empty()) {
			safe = false;
			goto next;
		}
		for (auto i = v->bb->begin(), e = v->bb->end(); i != e; ++i) {
			bool force = false;
			bool local = true;

			/*
			 * In general, function calls involve unknown effects and
			 * cannot be always included or excluded, unless they are
			 * explicitly marked as pure
			 */
			if (auto ci = dyn_cast<CallInst>(i)) {
				if (!ci->getCalledFunction())
					goto unsafe;

				if (ci->getCalledFunction()->getName().startswith("llvm."))
					goto safe;

				if (isPure(*ci->getCalledFunction()))
					goto safe;

			unsafe:
				force = true;
				goto out;
			safe:
				;
			}

			/*
			 * Variables without effects on the other BBs are safe to include
			 */
			for (auto ui = i->use_begin(), ue = i->use_end(); ui != ue; ++ui) {
				auto *user = dyn_cast<Instruction>(ui->getUser());
				if (user && user->getParent() != v->bb) {
					local = false;
					break;
				}
			}
			if (local)
				continue;

			if (dyn_cast<GetElementPtrInst>(i) ||
			    dyn_cast<LoadInst>(i) ||
			    dyn_cast<StoreInst>(i) ||
			    dyn_cast<CastInst>(i) ||
			    dyn_cast<CallInst>(i))
				goto out;

			continue;

		out:
			if (force || getUnderlyingType(i)->isStructTy()) {
				safe = false;
				break;
			}
		}
	next:
		v->safe_to_include = safe;
	}

	// Values that affect the return value are not safe to include/exclude
	std::list<Value*> ret_val;
	std::set<Value *> visited;
	ret_val.push_back(ret_inst);
	visited.insert(ret_inst);
	while (!ret_val.empty()) {
		Value *v = ret_val.front();
		ret_val.pop_front();
		if (!v)
			continue;

		Instruction *inst = dyn_cast<Instruction>(v);
		if (!inst)
			continue;

		vertices[bbmap[inst->getParent()]]->safe_to_include = false;
		/* If the value is used in predicates, we should preserve these
		 * bbs so that the predicates can be precisely tracked */
		for (auto i = inst->user_begin(), e = inst->user_end(); i != e; ++i)
			if (auto ci = dyn_cast<CmpInst>(*i))
				vertices[bbmap[ci->getParent()]]->safe_to_include = false;

		if (auto ri = dyn_cast<ReturnInst>(inst)) {
			Value *next = ri->getReturnValue();
			if (visited.insert(next).second)
				ret_val.push_back(next);
		} else if (auto pn = dyn_cast<PHINode>(inst)) {
			for (int i = 0, e = pn->getNumIncomingValues(); i != e; ++i) {
				Value *incoming = pn->getIncomingValue(i);
				/* The incoming block is critical when deciding the phinode value */
				vertices[bbmap[pn->getIncomingBlock(i)]]->safe_to_include = false;
				if (!dyn_cast<ConstantInt>(incoming) && visited.insert(incoming).second) {
					ret_val.push_back(incoming);
				}
			}
		}
	}
}

void path_iterator::reduce_paths() {
	if (!ForceExclude) {
		for (auto v : vertices) {
			if (!v->safe_to_include)
				continue;
			for (auto e : v->in_edges) {
				Vertex *pv = e->source;
				pv->directly_reachable.set(v->id);
				pv->indirectly_reachable |= v->directly_reachable;
				pv->indirectly_reachable |= v->indirectly_reachable;
			}
		}

		for (auto v : vertices) {
			auto i = v->in_edges.begin();
			while (i != v->in_edges.end()) {
				Edge *e = *i;
				Vertex *pv = e->source;
				if (pv->indirectly_reachable[v->id]) {
					gerase(pv->out_edges, e);
					i = v->in_edges.erase(i);
				} else {
					++i;
				}
			}
		}
	} else {
		for (auto v : vertices) {
			if (!v->safe_to_include)
				continue;
			if (v->in_edges.empty() || v->out_edges.empty())
				continue;

			// Unlink this Vertex from its preds and succs
			for (auto e : v->in_edges)
				gerase(e->source->out_edges, e);
			for (auto e : v->out_edges)
				gerase(e->target->in_edges, e);

			for (auto in_e : v->in_edges)
				for (auto out_e : v->out_edges)
					connect_edge(in_e, out_e);
		}
	}
}

void path_iterator::count_recursive(Vertex *v, BitVector &bv) {
	bv.set(v->id);
	if (v->in_edges.empty()) {
		v->_count = 1;
		return;
	}

	for (auto e : v->in_edges) {
		Vertex *source = e->source;
		if (!bv.test(source->id))
			count_recursive(source, bv);
	}

	for (auto e : v->in_edges)
		v->_count += e->source->_count;
}

int path_iterator::count_internal() {
	/*
	 * NOTE: This is just an approximation of simple paths in the graph. It
	 * considers the graph as a DAG by randomly ignoring back edges in the
	 * DFS.
	 */

	for (auto v : vertices)
		v->_count = 0;

	llvm::BitVector visited(vertices.size());
	count_recursive(exit, visited);
	return exit->_count;
}

path_iterator path_iterator::begin(Function &F, Context &c) {
	return path_iterator(F, c, true);
}

path_iterator path_iterator::end(Function &F, Context &c) {
	return path_iterator(F, c);
}

path_iterator &path_iterator::operator++() {
	if (vertices.size() == 1) {
		path.clear();
		return *this;
	}

	do {
		if (!switch_edge() || !fill_path()) {
			assert(path.empty());
			break;
		}
	} while (!is_feasible_path());

	return *this;
}

bool path_iterator::operator!=(const path_iterator &x) const {
	return ! (path == x.path);
}

std::set<BasicBlock*> &path_iterator::operator*() {
	bbpath.clear();
	if (path.empty()) {
		// Return all basic blocks if we have no path info yet
		for (auto i = F.begin(), e = F.end(); i != e; ++i)
			bbpath.insert(i);
		goto exit;
	}

	bbpath.insert(vertices.front()->bb);
	if (vertices.size() <= 1)
		goto exit;

	for (auto e : path) {
		for_each_bit(e->dominated_edges, bit)
			bbpath.insert(edges[bit]->target->bb);
		bbpath.insert(e->target->bb);
	}
exit:
	return bbpath;
}

std::list<path_iterator::Edge*> &path_iterator::get_path() {
	return path;
}

llvm::BasicBlock *path_iterator::get_entry() {
	return vertices.front()->bb;
}

rid::Formula path_iterator::path_condition() {
	Formula pc = True::get(c);
	int i = 0;
	for (auto e : path) {
		if (e != NULL) {
			ResolvePhiNodes rpn(c, this, e);
			pc = pc && rpn.visit(e->cond);
		}
	}
	pc = pc->deep_simplify();
	return pc;
}

llvm::Value *path_iterator::determine_phinode(llvm::PHINode *phi) {
	if (path.empty())
		return NULL;

	llvm::Value *ret = NULL;
	BasicBlock *parent = phi->getParent();
	for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
		BasicBlock *bb = phi->getIncomingBlock(i);
		Edge *edge = NULL;
		for (auto e : edges)
			if (e->source->bb == bb && e->target->bb == parent) {
				edge = e;
				break;
			}
		assert(edge && edge->id < nr_original_edges);
		if (!path_dominated_edges.test(edge->id))
			continue;
		if (!ret) {
			ret = phi->getIncomingValue(i);
		} else {
			ret = NULL;
			break;
		}
	}
	return ret;
}

llvm::Value *path_iterator::determine_phinode(llvm::PHINode *phi, Edge *edge) {
	if (!phi || !edge)
		return NULL;

	PHINode *newphi = NULL;
	for (auto i = grfind(path, edge), e = path.rend(); i != e; ++i) {
		bool done = true;
		do {
			done = true;
			try {
				BitVector &bv = (*i)->phi_values.at(phi);
				if (bv.count() == 1) {
					Value *v = phi->getIncomingValue(bv.find_first());
					newphi = dyn_cast<PHINode>(v);
					if (!newphi)
						return v;
					if (newphi == phi)
						return v;
					phi = newphi;
					done = false;
				}
			} catch (std::out_of_range &e) {}
		} while (!done);
	}

	return NULL;
}

int path_iterator::nr_branches() {
	return branches;
}

int path_iterator::count() {
	return count_before_slicing;
}

int path_iterator::count_sliced() {
	return count_after_slicing;
}

void path_iterator::pretty_print_path(llvm::raw_ostream &os, std::list<Edge*> &p) {
	BasicBlock *last_bb = NULL, *entry = get_entry();

	if (entry) {
		entry->printAsOperand(os, false);
		last_bb = entry;
	}
	for (auto edge : p) {
		if (!edge)
			continue;

		if (edge->source->bb != last_bb) {
			os << " ";
			edge->source->bb->printAsOperand(os, false);
		}

		os << " ";
		edge->target->bb->printAsOperand(os, false);
		last_bb = edge->target->bb;
	}
}

void path_iterator::dump_to_dot(const std::string &filename) {
	std::error_code ec;
	raw_fd_ostream file(filename, ec, sys::fs::F_Text);

	if (ec) {
		errs() << "Cannot write to " << filename << ": " << ec.message() << "\n";
		return;
	}

	file << "digraph \"CFG\" {\n";
	for (auto v : vertices) {
		if (ForceExclude && v->safe_to_include)
			continue;

		file << "\tNode" << v << "[";
		if (v->safe_to_include)
			file << "color=\"green\",";
		else
			file << "color=\"blue\",";
		file << "label=\"" << v->name << "\"];\n";
		for (auto e : v->in_edges) {
			file << "\tNode" << e->source << " -> Node" << e->target << "[";
			if (gexist(path, e))
				file << "color=\"red\",";
			else
				file << "color=\"grey\",";
			file << "label=\"";
			if (e->cond.get() != NULL)
				file << e->cond;
			else
				file << "True";
			file << "\"";
			file << "];\n";
		}
	}
	file << "}\n";
}

void path_iterator::plot_path_pair(const std::string &filename, std::list<Edge*> &p1, std::list<Edge*> &p2) {
	std::error_code ec;
	raw_fd_ostream file(filename, ec, sys::fs::F_Text);

	if (ec) {
		errs() << "Cannot write to " << filename << ": " << ec.message() << "\n";
		return;
	}

	file << "digraph \"CFG\" {\n";
	file << "\trandkdir=LR;\n";
	file << "\tnode [shape=box];\n";
	for (auto v : vertices) {
		if (ForceExclude && v->safe_to_include)
			continue;

		file << "\tNode" << v << "[";
		if (v->safe_to_include)
			file << "color=\"grey\",";
		else
			file << "color=\"black\",";
		file << "label=<";
		file << v->bb->getName() << ":<br align=\"left\" />\n";
		std::string inst;
		raw_string_ostream os(inst);
		for (auto i = v->bb->begin(), e = v->bb->end(); i != e; ++i) {
			inst.clear();
			i->print(os);
			os.flush();
			if (inst.size() < 43)
				file << inst << "<br align=\"left\" />\n";
			else
				file << inst.substr(0, 40) << "...<br align=\"left\" />\n";
		}
		file << ">";
		file << "];\n";
		for (auto e : v->out_edges) {
			file << "\tNode" << e->source << " -> Node" << e->target << "[";
			if (gexist(p1, e)) {
				if (gexist(p2, e))
					file << "color=\"green\"";
				else
					file << "color=\"cyan\"";
			} else {
				if (gexist(p2, e))
					file << "color=\"yellow\"";
				else
					file << "color=\"grey\"";
			}
			file << "];\n";
		}
	}
	file << "}\n";
}

};
