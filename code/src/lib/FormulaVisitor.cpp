#include "FormulaVisitor.h"

#include <list>
#include <algorithm>

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

#include "PathIterator.h"

using namespace rid;
using namespace llvm;

namespace rid {

Formula FormulaVisitor::visit_aux(Formula F) {
	Formula intermediate;
	Formula ret;

	switch (F->get_type()) {
	case __Formula::T_True:
		intermediate = pre_visit_true(std::static_pointer_cast<True>(F));
		break;
	case __Formula::T_False:
		intermediate = pre_visit_false(std::static_pointer_cast<False>(F));
		break;
	case __Formula::T_Atom:
		intermediate = pre_visit_atom(std::static_pointer_cast<Atom>(F));
		break;
	case __Formula::T_Conjunction:
		intermediate = pre_visit_conj(std::static_pointer_cast<Conjunction>(F));
		break;
	case __Formula::T_Disjunction:
		intermediate = pre_visit_disj(std::static_pointer_cast<Disjunction>(F));
		break;
	case __Formula::T_Negation:
		intermediate = pre_visit_neg(std::static_pointer_cast<Negation>(F));
		break;
	}

	switch (intermediate->get_type()) {
	case __Formula::T_True:
		ret = post_visit_true(std::static_pointer_cast<True>(intermediate));
		break;
	case __Formula::T_False:
		ret = post_visit_false(std::static_pointer_cast<False>(intermediate));
		break;
	case __Formula::T_Atom:
		ret = post_visit_atom(std::static_pointer_cast<Atom>(intermediate));
		break;
	case __Formula::T_Conjunction:
	{
		auto f = std::static_pointer_cast<Conjunction>(intermediate);
		auto p = mid_visit_conj(f, visit_aux(f->p));
		ret = post_visit_conj(f, p, visit_aux(f->q));
		break;
	}
	case __Formula::T_Disjunction:
	{
		auto f = std::static_pointer_cast<Disjunction>(intermediate);
		auto p = mid_visit_disj(f, visit_aux(f->p));
		ret = post_visit_disj(f, p, visit_aux(f->q));
		break;
	}
	case __Formula::T_Negation:
	{
		auto f = std::static_pointer_cast<Negation>(intermediate);
		ret = post_visit_neg(f, visit_aux(f->p));
		break;
	}
	}

	return ret;
}

Formula FormulaVisitor::visit(Formula F) {
	if (F.get() == NULL)
		return True::get(c);
	Formula ret;
	initialize(F);
	ret = visit_aux(F);
	finalize(ret);
	return ret;
}

/******************************************************************************
 * PrintTree
 ******************************************************************************/

void PrintTree::print_prefix() {
	for (int i = 0; i < level; ++i)
		errs() << "    ";
}

Formula PrintTree::visit_true(std::shared_ptr<True> F) {
	print_prefix();
	errs() << "True\n";
	return F;
}

Formula PrintTree::visit_false(std::shared_ptr<False> F) {
	print_prefix();
	errs() << "False\n";
	return F;
}

Formula PrintTree::visit_atom(std::shared_ptr<Atom> F) {
	print_prefix();
	errs() << "Atom: " << Atom::OP_SYMBOL[F->op] << " ";
	if (auto c = dynamic_cast<Constant*>(F->lhs))
		errs() << "Constant(" << c->i << ")";
	else if (auto s = dynamic_cast<Signature*>(F->lhs))
		errs() << "Signature(" << s->sig << ")";
	else if (auto v = dynamic_cast<Variable*>(F->lhs)) {
		errs() << "Variable(";
		v->print(errs());
		errs() << ")";
	} else
		errs() << "??";
	errs() << " ";
	if (auto c = dynamic_cast<Constant*>(F->rhs))
		errs() << "Constant(" << c->i << ")";
	else if (auto s = dynamic_cast<Signature*>(F->rhs))
		errs() << "Signature(" << s->sig << ")";
	else if (auto v = dynamic_cast<Variable*>(F->rhs)) {
		errs() << "Variable(";
		v->print(errs());
		errs() << ")";
	} else
		errs() << "??";
	errs() << "\n";

	return F;
}

Formula PrintTree::pre_visit_conj(std::shared_ptr<Conjunction> F) {
	print_prefix();
	errs() << "Conjunction\n";
	level ++;
	return F;
}

Formula PrintTree::post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q) {
	level --;
	return F;
}

Formula PrintTree::pre_visit_disj(std::shared_ptr<Disjunction> F) {
	print_prefix();
	errs() << "Disjunction\n";
	level ++;
	return F;
}

Formula PrintTree::post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q) {
	level --;
	return F;
}

Formula PrintTree::pre_visit_neg(std::shared_ptr<Negation> F) {
	print_prefix();
	errs() << "Negation\n";
	level ++;
	return F;
}

Formula PrintTree::post_visit_neg(std::shared_ptr<Negation> F, Formula p) {
	level --;
	return F;
}

/******************************************************************************
 * ResolvePhiNodes
 ******************************************************************************/

Formula ResolvePhiNodes::post_visit_atom(std::shared_ptr<Atom> F) {
	PHINode *phi = F->v ? dyn_cast<PHINode>(F->v) : NULL;
	Value *v;
	std::shared_ptr<Atom> atom = F;

	if (phi) {
		v = path->determine_phinode(phi, cur);
		assert(v);
		if (auto ci = dyn_cast<ConstantInt>(v)) {
			if (ci->isZero())
				return False::get(c);
			if (ci->isOne())
				return True::get(c);
		}
		atom = std::dynamic_pointer_cast<Atom>(c.get_atom(v));
	}

	Operand *lhs = atom->lhs, *rhs = atom->rhs;
	// It is safe to dynamic_cast NULL pointers, which is different from llvm::dyn_cast.
	Variable *vlhs = dynamic_cast<Variable*>(lhs), *vrhs = dynamic_cast<Variable*>(rhs);

	while (vlhs && vlhs->v) {
		phi = dyn_cast<PHINode>(vlhs->v);
		if (!phi)
			break;
		v = path->determine_phinode(phi, cur);
		assert(v);
		if (v == vlhs->v)
			break;
		lhs = c.get_operand(v);
		vlhs = dynamic_cast<Variable*>(lhs);
	}

	while (vrhs && vrhs->v) {
		phi = dyn_cast<PHINode>(vrhs->v);
		if (!phi)
			break;
		v = path->determine_phinode(phi, cur);
		assert(v);
		if (v == vrhs->v)
			break;
		rhs = c.get_operand(v);
		vrhs = dynamic_cast<Variable*>(rhs);
	}

	if (lhs == atom->lhs && rhs == atom->rhs)
		return atom;
	return Formula(new Atom(c, atom->op, lhs, rhs));
}

Formula ResolvePhiNodes::post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q) {
	if (p == F->p && q == F->q)
		return F;
	return p && q;
}

Formula ResolvePhiNodes::post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q) {
	if (p == F->p && q == F->q)
		return F;
	return p || q;
}

Formula ResolvePhiNodes::post_visit_neg(std::shared_ptr<Negation> F, Formula p) {
	if (p == F->p)
		return F;
	return !p;
}

/******************************************************************************
 * VariableToValue
 ******************************************************************************/

Operand *VariableToValue::update_operand(Operand *op) {
	Variable *var = dynamic_cast<Variable*>(op);
	if (var)
		op = c.get_operand(sigmap[var->v].get());

	if (eop) {
		// Local variable that has no signature
		Variable *var = dynamic_cast<Variable*>(op);
		if (var) {
			std::string sig;
			raw_string_ostream os(sig);
			os << "{";
			var->v->printAsOperand(os, false);
			os << "@" << getFunctionName(c.F) << "}";
			os.flush();
			op = c.get_operand(sig);
		}

		// Signatures involving local variables
		Signature *sig = dynamic_cast<Signature*>(op);
		if (sig) {
			size_t left, right;
			left = sig->sig.find_first_of('<');
			if (left != std::string::npos) {
				std::string buf = sig->sig;
				while (left != std::string::npos) {
					right = buf.find_first_of('>');
					std::string var_name = buf.substr(left, right - left + 1);
					Variable *var = dynamic_cast<Variable*>(c.get_operand(var_name));
					bool updated = false;
					if (var && var->v) {
						StringRef updated_sig = sigmap[var->v].get();
						if (updated_sig.front() != '<') {
							replace_all(buf, var_name, updated_sig);
							updated = true;
						}
					}
					if (!updated) {
						std::string sig;
						raw_string_ostream os(sig);
						os << "{" << var_name.substr(1, right - left - 1) << "@" << getFunctionName(c.F) << "}";
						os.flush();
						replace_all(buf, var_name, sig);
					}
					left = buf.find_first_of('<');
				}
				op = c.get_operand(buf);
			}
		}
	}

	return op;
}

Formula VariableToValue::post_visit_atom(std::shared_ptr<Atom> F) {
	Operand *new_lhs = update_operand(F->lhs);
	Operand *new_rhs = update_operand(F->rhs);
	if (new_lhs == NULL || new_rhs == NULL)
		return NULL;
	if (F->lhs == new_lhs && F->rhs == new_rhs)
		return F;

	Atom *new_atom = new Atom(c, F->op, new_lhs, new_rhs);
	return Formula(new_atom);
}

Formula VariableToValue::post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q) {
	if (p == F->p && q == F->q)
		return F;
	return p && q;
}

Formula VariableToValue::post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q) {
	if (p == F->p && q == F->q)
		return F;
	return p || q;
}

Formula VariableToValue::post_visit_neg(std::shared_ptr<Negation> F, Formula p) {
	if (p == F->p)
		return F;
	return !p;
}

/******************************************************************************
 * RangeToConstant
 ******************************************************************************/

void RangeToConstant::initialize(Formula F) {
	left.emplace_back();
	current = &left.back();
}

Formula RangeToConstant::post_visit_atom(std::shared_ptr<Atom> F) {
	Atom::Operator op = F->op;
	Operand *lhs = F->lhs, *rhs = F->rhs;
	Signature *sig = NULL;
	Constant *cnt = NULL;

	// Normalize to: Signature(...) <op> Constant(...)
	if (lhs->is_signature() && rhs->is_constant()) {
		sig = dynamic_cast<Signature*>(lhs);
		cnt = dynamic_cast<Constant*>(rhs);
	} else if (lhs->is_constant() && rhs->is_signature()) {
		switch (op) {
		case Atom::OP_LT: op = Atom::OP_GT; break;
		case Atom::OP_LE: op = Atom::OP_GE; break;
		case Atom::OP_GT: op = Atom::OP_LT; break;
		case Atom::OP_GE: op = Atom::OP_LE; break;
		}
		sig = dynamic_cast<Signature*>(rhs);
		cnt = dynamic_cast<Constant*>(lhs);
	}

	if (!sig)
		return F;

	Range &r = (*current)[sig];
	switch(op) {
	case Atom::OP_LT: r.max = std::min(r.max, cnt->i - 1); break;
	case Atom::OP_LE: r.max = std::min(r.max, cnt->i); break;
	case Atom::OP_GT: r.min = std::max(r.min, cnt->i + 1); break;
	case Atom::OP_GE: r.min = std::max(r.min, cnt->i); break;
	}
	r.atoms.push_back(F.get());

	return F;
}

Formula RangeToConstant::pre_visit_conj(std::shared_ptr<Conjunction> F) {
	left.emplace_back();
	right.emplace_back();
	current = &left.back();
	return F;
}

Formula RangeToConstant::mid_visit_conj(std::shared_ptr<Conjunction> F, Formula p) {
	current = &right.back();
	return p;
}

Formula RangeToConstant::post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q) {
	std::map<Signature*, Range> *left = &this->left.back(), *right = &this->right.back();
	current = &*(++this->left.rbegin());

	for (auto &p : *left) {
		Signature *op = p.first;
		Range &r = p.second;
		Range &cr = (*current)[op];
		cr.min = std::max(cr.min, r.min);
		cr.max = std::min(cr.max, r.max);
		for (auto atom : r.atoms)
			cr.atoms.push_back(atom);
	}

	for (auto &p : *right) {
		Signature *op = p.first;
		Range &r = p.second;
		Range &cr = (*current)[op];
		cr.min = std::max(cr.min, r.min);
		cr.max = std::min(cr.max, r.max);
		for (auto atom : r.atoms)
			cr.atoms.push_back(atom);
	}

	this->left.pop_back();
	this->right.pop_back();
	return F;
}

Formula RangeToConstant::pre_visit_disj(std::shared_ptr<Disjunction> F) {
	left.emplace_back();
	right.emplace_back();
	current = &left.back();
	return F;
}

Formula RangeToConstant::mid_visit_disj(std::shared_ptr<Disjunction> F, Formula p) {
	current = &right.back();
	return p;
}

Formula RangeToConstant::post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q) {
	std::map<Signature*, Range> *left = &this->left.back(), *right = &this->right.back();
	current = &*(++this->left.rbegin());

	for (auto &p : *left) {
		Signature *op = p.first;
		Range &lr = p.second;
		try {
			Range &rr = right->at(op);
			Range &cr = (*current)[op];
			cr.min = std::min(lr.min, cr.min);
			cr.max = std::max(lr.max, cr.max);
			for (auto atom : lr.atoms)
				cr.atoms.push_back(atom);
			for (auto atom : rr.atoms)
				cr.atoms.push_back(atom);
		} catch (std::out_of_range &e) {}
	}

	this->left.pop_back();
	this->right.pop_back();
	return F;
}

Formula RangeToConstant::pre_visit_neg(std::shared_ptr<Negation> F) {
	left.emplace_back();
	current = &left.back();
	return F;
}

Formula RangeToConstant::post_visit_neg(std::shared_ptr<Negation> F, Formula p) {
	std::map<Signature*, Range> *left = &this->left.back();
	current = &*(++this->left.rbegin());

	for (auto &p : *left) {
		Signature *op = p.first;
		Range &r = p.second;
		Range &cr = (*current)[op];
		if (r.max == INT_MAX) {
			cr.max = r.min - 1;
		} else if (r.min == INT_MIN) {
			cr.min = r.max + 1;
		}
		for (auto atom : r.atoms)
			cr.atoms.push_back(atom);
	}

	this->left.pop_back();
	return F;
}

void RangeToConstant::finalize(Formula F) {
	assert(left.size() == 1);
	assert(right.empty());
	current = &left.back();

	for (auto &p : *current) {
		Signature *op = p.first;
		Range &r = p.second;
		if (r.min != r.max)
			continue;
		if (op->sig == "[0]") {
			raw_string_ostream os(ret);
			os << r.min;
			// These atoms should be thrown away during simplification
			for (auto atom : r.atoms) {
				atom->op = Atom::OP_EQ;
				atom->lhs = atom->rhs = c.get_operand(r.min);
			}
		} else {
			for (auto atom : r.atoms) {
				atom->op = Atom::OP_EQ;
				atom->lhs = op;
				atom->rhs = c.get_operand(r.min);
			}
		}
	}

	current = NULL;
	left.clear();
}


/******************************************************************************
 * RemoveLocals
 ******************************************************************************/

/* The following way of removing clauses on local variables and constants are
 * far from being precise. Most of all, constraints on arguments or return
 * values expressed by local variables (e.g. ([0] = {var}) /\ ({var} = 4)) are
 * discarded.
 *
 * TODO: Find a more precise way of removing local variables.
 */

Formula RemoveLocals::post_visit_atom(std::shared_ptr<Atom> F) {
	bool remove = true;

	if (auto lhs = dynamic_cast<Signature*>(F->lhs))
		if (lhs->sig.find('[') != std::string::npos)
			remove = false;

	if (auto rhs = dynamic_cast<Signature*>(F->rhs))
		if (rhs->sig.find('[') != std::string::npos)
			remove = false;

	if (remove)
		return NULL;
	return F;
}

Formula RemoveLocals::post_visit_conj(std::shared_ptr<Conjunction> F, Formula p, Formula q) {
	if (p.get() == NULL)
		return q;
	if (q.get() == NULL)
		return p;
	return F;
}

Formula RemoveLocals::post_visit_disj(std::shared_ptr<Disjunction> F, Formula p, Formula q) {
	if (p.get() == NULL)
		return q;
	if (q.get() == NULL)
		return p;
	return F;
}

Formula RemoveLocals::post_visit_neg(std::shared_ptr<Negation> F, Formula p) {
	if (p.get() == NULL)
		return NULL;
	return F;
}

};
