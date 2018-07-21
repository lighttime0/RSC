//===---- FormulaVisitor.h - Visitors and transformers of formulas -*- C++ -*-===//

#ifndef FORMULAVISITOR_H
#define FORMULAVISITOR_H

#include <climits>
#include <map>
#include <list>

#include "Formula.h"
#include "Summary.h"
#include "PathIterator.h"

namespace rsc {

class FormulaVisitor {
	Formula visit_aux(Formula F);

protected:
	Context &c;

	virtual void initialize(Formula F) {}

	virtual Formula pre_visit_true(std::shared_ptr<True> F) { return F; }
	virtual Formula post_visit_true(std::shared_ptr<True> F) { return F; }

	virtual Formula pre_visit_false(std::shared_ptr<False> F) { return F; }
	virtual Formula post_visit_false(std::shared_ptr<False> F) { return F; }

	virtual Formula pre_visit_atom(std::shared_ptr<Atom> F) { return F; }
	virtual Formula post_visit_atom(std::shared_ptr<Atom> F) { return F; }

	virtual Formula pre_visit_conj(std::shared_ptr<Conjunction> F) { return F; }
	virtual Formula mid_visit_conj(std::shared_ptr<Conjunction> F, Formula p) { return p; }
	virtual Formula post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q) { return F; }

	virtual Formula pre_visit_disj(std::shared_ptr<Disjunction> F) { return F; }
	virtual Formula mid_visit_disj(std::shared_ptr<Disjunction> F, Formula p) { return p; }
	virtual Formula post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q) { return F; }

	virtual Formula pre_visit_neg(std::shared_ptr<Negation> F) { return F; }
	virtual Formula post_visit_neg(std::shared_ptr<Negation> F, Formula p) { return F; }

	virtual void finalize(Formula F) {}

public:
	FormulaVisitor(Context &c) : c(c) {}
	virtual ~FormulaVisitor() {}

	Formula visit(Formula F);
};

class PrintTree : public FormulaVisitor {
	int level;

	void print_prefix();

protected:
	virtual Formula visit_true(std::shared_ptr<True> F);

	virtual Formula visit_false(std::shared_ptr<False> F);

	virtual Formula visit_atom(std::shared_ptr<Atom> F);

	virtual Formula pre_visit_conj(std::shared_ptr<Conjunction> F);
	virtual Formula post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q);

	virtual Formula pre_visit_disj(std::shared_ptr<Disjunction> F);
	virtual Formula post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q);

	virtual Formula pre_visit_neg(std::shared_ptr<Negation> F);
	virtual Formula post_visit_neg(std::shared_ptr<Negation> F, Formula p);

public:
	PrintTree(Context &c) : FormulaVisitor(c), level(0) {}
	virtual ~PrintTree() {}
};

class ResolvePhiNodes : public FormulaVisitor {
	path_iterator *path;
	path_iterator::Edge *cur;

protected:
	virtual Formula post_visit_atom(std::shared_ptr<Atom> F);
	virtual Formula post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q);
	virtual Formula post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q);
	virtual Formula post_visit_neg(std::shared_ptr<Negation> F, Formula p);

public:
	ResolvePhiNodes(Context &c, path_iterator *p, path_iterator::Edge *ce)
		: FormulaVisitor(c), path(p), cur(ce) {}
	virtual ~ResolvePhiNodes() {}
};

class VariableToValue : public FormulaVisitor {
	SignatureMap &sigmap;
	bool eop;

	Operand *update_operand(Operand *op);

protected:
	virtual Formula post_visit_atom(std::shared_ptr<Atom> F);
	virtual Formula post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q);
	virtual Formula post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q);
	virtual Formula post_visit_neg(std::shared_ptr<Negation> F, Formula p);

public:
	VariableToValue(Context &c, SignatureMap &sigmap) : FormulaVisitor(c), sigmap(sigmap), eop(false) {}
	virtual ~VariableToValue() {}

	void end_of_path() { eop = true; };
};

class RangeToConstant : public FormulaVisitor {
	struct Range {
		long long min, max;
		std::list<Atom*> atoms;
		Range() : min(INT_MIN), max(INT_MAX) {}
	};

	std::list<std::map<Signature*, Range>> left, right;
	std::map<Signature*, Range> *current;

	std::string ret;

protected:
	virtual void initialize(Formula F);

	virtual Formula post_visit_atom(std::shared_ptr<Atom> F);

	virtual Formula pre_visit_conj(std::shared_ptr<Conjunction> F);
	virtual Formula mid_visit_conj(std::shared_ptr<Conjunction> F, Formula p);
	virtual Formula post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q);

	virtual Formula pre_visit_disj(std::shared_ptr<Disjunction> F);
	virtual Formula mid_visit_disj(std::shared_ptr<Disjunction> F, Formula p);
	virtual Formula post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q);

	virtual Formula pre_visit_neg(std::shared_ptr<Negation> F);
	virtual Formula post_visit_neg(std::shared_ptr<Negation> F, Formula p);

	virtual void finalize(Formula F);

public:
	RangeToConstant(Context &c) : FormulaVisitor(c) {}
	virtual ~RangeToConstant() {}

	llvm::StringRef get_return() { return ret; }
};

class RemoveLocals : public FormulaVisitor {
protected:
	virtual Formula post_visit_atom(std::shared_ptr<Atom> F);
	virtual Formula post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q);
	virtual Formula post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q);
	virtual Formula post_visit_neg(std::shared_ptr<Negation> F, Formula p);

public:
	RemoveLocals(Context &c) : FormulaVisitor(c) {}
	virtual ~RemoveLocals() {}
};

};

#endif /* FORMULAVISITOR_H */
