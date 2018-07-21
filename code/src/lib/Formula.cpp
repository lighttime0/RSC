#include "Formula.h"

#include <iostream>
#include <vector>

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

#include "util.h"

using namespace llvm;

namespace rid {

Context::~Context() {
	for (auto op : operands)
		delete op;
	for (auto &pair : variables)
		delete pair.second;
}

void Context::operand_is_legal(Operand *op) {
	if (gexist(operands, op)) {
		errs() << "Found in @operands\n";
	} else {
		bool found = false;
		for (auto &p : variables) {
			if (p.second == op) {
				found = true;
				break;
			}
		}
		if (found)
			errs() << "Found in @variables\n";
		else
			errs() << "Not found!\n";
	}
}

Operand *Constant::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	Expr *ret = sub(c, this);
	if (ret)
		return static_cast<Operand*>(ret);
	return c.get_operand(this->i);
}

Operand *Variable::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	assert(0 && "Variables cannot be copied!");
	return NULL;
}

Operand *Signature::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	Expr *ret = sub(c, this);
	if (ret)
		return static_cast<Operand*>(ret);
	return c.get_operand(this->sig);
}

Constant *Context::get_constant(long long c) {
	try {
		return constants.at(c);
	} catch (std::out_of_range &e) {}

	Constant *op = new Constant(*this);
	op->i = c;
	operands.push_back(op);
	constants[c] = op;
	return op;
}

Variable *Context::get_variable(llvm::Value *v) {
	try {
		return variables.at(v);
	} catch (std::out_of_range &e) {}

	Variable *op = new Variable(*this);
	op->v = v;
	raw_string_ostream os(op->name);
	os << "<";
	v->printAsOperand(os, false);
	os << ">";
	os.flush();
	variables[v] = op;
	name_to_variables[op->name] = op;
	return op;
}

Variable *Context::get_variable(const std::string &name) {
	try {
		return name_to_variables.at(name);
	} catch (std::out_of_range &e) {
		errs() << "Try getting a non-exist variable: " << name << "\n";
		assert(0);
	}

	return NULL;
}

Signature *Context::get_signature(const std::string &sig) {
	try {
		return signatures.at(sig);
	} catch (std::out_of_range &e) {}

	Signature *op = new Signature(*this);
	op->sig = sig;
	operands.push_back(op);
	signatures[sig] = op;
	return op;
}

Operand *Context::get_operand(llvm::Value *v) {
	if (auto ci = dyn_cast<ConstantInt>(v)) {
		return get_constant(ci->getSExtValue());
	} else if (auto cpn = dyn_cast<ConstantPointerNull>(v)) {
		return get_constant(0);
	} else if (auto ce = dyn_cast<ConstantExpr>(v)) {
		if (ce->isCast()) {
			Value *op = ce->getOperand(0);
			if (auto cc = dyn_cast<llvm::Constant>(op))
				return get_operand(cc);
		}
	}

	return get_variable(v);
}

Operand *Context::get_operand(long long i) {
	return get_constant(i);
}

Operand *Context::get_operand(const char *sig) {
	std::string s(sig);
	return get_operand(s);
}

Operand *Context::get_operand(const std::string &sig) {
	auto i = atoi(sig);

	if (i.first)
		return get_constant(i.second);
	if (*sig.begin() == '<' && *sig.rbegin() == '>') {
		return get_variable(sig);
	}
	return get_signature(sig);
}

Operand *Context::get_operand(Operand *op, std::function<Expr*(Context&, Expr*)> sub) {
	Operand *ret = op->deep_copy(*this, sub);
	return ret;
}

Formula Context::get_atom(llvm::Value *v) {
	try {
		return value_to_atoms.at(v);
	} catch (std::out_of_range &e) {}

	Atom *atom = new Atom(*this, v);
	Formula ret = Formula(atom);
	value_to_atoms[v] = ret;
	if (atom->op == Atom::OP_NULL && !atom->name.empty())
		name_to_atoms[atom->name] = ret;
	return ret;
}

Formula Context::get_atom(const std::string &name) {
	try {
		return name_to_atoms.at(name);
	} catch (std::out_of_range &e) {}
	assert(0 && "Attempt to get an atom by name which is previously unknown!");
	return NULL;
}

Operand *Context::parse_to_operand(Z3_ast ast) {
	Z3_ast_kind kind = Z3_get_ast_kind(z3, ast);
	Z3_app app;
	Z3_decl_kind decl;
	int i;

	switch (kind) {
	case Z3_NUMERAL_AST:
		if (!Z3_get_numeral_int(z3, ast, &i))
			return NULL;
		return get_operand(i);
	case Z3_APP_AST:
		app = Z3_to_app(z3, ast);
		decl = Z3_get_decl_kind(z3, Z3_get_app_decl(z3, app));
		if (decl == Z3_OP_UNINTERPRETED) {
			Z3_symbol sym = Z3_get_decl_name(z3, Z3_get_app_decl(z3, app));
			switch(Z3_get_symbol_kind(z3, sym)) {
			case Z3_INT_SYMBOL:
				return get_operand(Z3_get_symbol_int(z3, sym));
			case Z3_STRING_SYMBOL:
				return get_operand(Z3_get_symbol_string(z3, sym));
			}
		}
		errs() << "Unknown decl type for operand: " << decl << "\n";
		return NULL;
	}

	errs() << "Unknown ast type for operand: " << kind << "\n";
	return NULL;
}

Formula Context::parse_to_formula(Z3_ast ast) {
	Z3_ast_kind kind = Z3_get_ast_kind(z3, ast);
	Z3_app app;
	Z3_decl_kind decl;
	Formula p, q;
	Operand *l, *r;
	Atom::Operator op;

	switch (kind) {
	case Z3_APP_AST:
		app = Z3_to_app(z3, ast);
		decl = Z3_get_decl_kind(z3, Z3_get_app_decl(z3, app));
		switch(decl) {
		case Z3_OP_TRUE:
			return True::get(*this);
		case Z3_OP_FALSE:
			return False::get(*this);
		case Z3_OP_AND:
			p = True::get(*this);
			for (int i = 0, e = Z3_get_app_num_args(z3, app); i != e; ++i) {
				q = parse_to_formula(Z3_get_app_arg(z3, app, i));
				if (q.get() == NULL)
					return NULL;
				p = p && q;
			}
			return p;
		case Z3_OP_OR:
			p = False::get(*this);
			for (int i = 0, e = Z3_get_app_num_args(z3, app); i != e; ++i) {
				q = parse_to_formula(Z3_get_app_arg(z3, app, i));
				if (q.get() == NULL)
					return NULL;
				p = p || q;
			}
			return p;
		case Z3_OP_NOT:
			p = parse_to_formula(Z3_get_app_arg(z3, app, 0));
			if (p.get() == NULL)
				return NULL;
			return (!p);
		case Z3_OP_EQ:
			p = True::get(*this);
			l = parse_to_operand(Z3_get_app_arg(z3, app, 0));
			if (!l)
				return NULL;
			for (int i = 1, e = Z3_get_app_num_args(z3, app); i != e; ++i) {
				r = parse_to_operand(Z3_get_app_arg(z3, app, i));
				if (!r)
					return NULL;
				q = Formula(new Atom(*this, Atom::OP_EQ, l, r));
				p = p && q;
			}
			return p;
		case Z3_OP_DISTINCT:
		{
			std::vector<Operand*> ops(Z3_get_app_num_args(z3, app));
			for (int i = 0, e = Z3_get_app_num_args(z3, app); i != e; ++i) {
				l = parse_to_operand(Z3_get_app_arg(z3, app, i));
				if (!l)
					return NULL;
				ops[i] = l;
			}
			p = True::get(*this);
			for (int i = 0, e = Z3_get_app_num_args(z3, app); i != e; ++i) {
				for (int j = i+1, f = Z3_get_app_num_args(z3, app); j != f; ++j) {
					q = Formula(new Atom(*this, Atom::OP_NE, ops[i], ops[j]));
					p = p && q;
				}
			}
			return q;
		}
		case Z3_OP_LE:
			op = Atom::OP_LE;
			goto parse;
		case Z3_OP_GE:
			op = Atom::OP_GE;
			goto parse;
		case Z3_OP_LT:
			op = Atom::OP_LT;
			goto parse;
		case Z3_OP_GT:
			op = Atom::OP_GT;
parse:
			l = parse_to_operand(Z3_get_app_arg(z3, app, 0));
			r = parse_to_operand(Z3_get_app_arg(z3, app, 1));
			if (!l || !r)
				return NULL;
			return Formula(new Atom(*this, op, l, r));
		case Z3_OP_UNINTERPRETED:
			Z3_symbol sym = Z3_get_decl_name(z3, Z3_get_app_decl(z3, app));
			if (Z3_get_symbol_kind(z3, sym) == Z3_STRING_SYMBOL) {
				try {
					return get_atom(Z3_get_symbol_string(z3, sym));
				} catch (std::out_of_range &e) {}
			}
			break;
		}
		errs() << "Unknown decl kind for formula: " << decl << "\n";
		errs() << Z3_ast_to_string(z3, ast) << "\n";
		break;
	}

	errs() << "Unknown ast kind for formula: " << kind << "\n";
	return NULL;
}

Formula Context::parse(z3::expr e) {
	return parse_to_formula(e);
}

void Context::dump_var_bindings() {
	for (auto &pair : variables) {
		llvm::Value *v = pair.first;
		Variable *var = pair.second;
		errs() << "    ";
		v->printAsOperand(errs(), false);
		errs() << " -> ";
		var->print(errs());
		errs() << "\n";
	}
}

Formula __Formula::simplify() {
	// simplify tactic from z3
	z3::goal g(c.z3);
	g.add(z3_expr());
	z3::tactic t(c.z3, "simplify");
	z3::apply_result r = t(g);
	assert(r.size() == 1);
	return c.parse(r[0].as_expr());
}

Formula __Formula::deep_simplify() {
	// ctx-solver-simplify tactic from z3
	z3::goal g(c.z3);
	g.add(z3_expr());
	z3::tactic t(c.z3, "ctx-solver-simplify");
	z3::apply_result r = t(g);
	assert(r.size() == 1);
	return c.parse(r[0].as_expr());
}

bool __Formula::check() {
	z3::solver s(c.z3);
	z3::params p(c.z3);
	p.set(":timeout", 100u);
	s.set(p);
	s.add(z3_expr());
	if (s.check() == z3::unsat)
		return false;
	return true;
}

Formula True::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	return get(c);
}

Formula False::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	return get(c);
}

const char *Atom::OP_SYMBOL[Atom::OP_END] = {
	"",
	"=",
	"!=",
	"<",
	"<=",
	">",
	">="
};

Atom::Atom(Context &c, Value *v)
	: __Formula(c), op(OP_NULL), lhs(NULL), rhs(NULL), v(v) {
	if (v) {
		if (ICmpInst *II = dyn_cast<ICmpInst>(v)) {
			bool match = true;
			switch(II->getPredicate()) {
			case CmpInst::Predicate::ICMP_EQ:
				op = OP_EQ;
				break;
			case CmpInst::Predicate::ICMP_NE:
				op = OP_NE;
				break;
			case CmpInst::Predicate::ICMP_SGE:
			case CmpInst::Predicate::ICMP_UGE:
				op = OP_GE;
				break;
			case CmpInst::Predicate::ICMP_SLT:
			case CmpInst::Predicate::ICMP_ULT:
				op = OP_LT;
				break;
			case CmpInst::Predicate::ICMP_SGT:
			case CmpInst::Predicate::ICMP_UGT:
				op = OP_GT;
				break;
			case CmpInst::Predicate::ICMP_SLE:
			case CmpInst::Predicate::ICMP_ULE:
				op = OP_LE;
				break;
			default:
				match = false;
			}
			if (match) {
				Value *left = II->getOperand(0), *right = II->getOperand(1);
				lhs = c.get_operand(left);
				rhs = c.get_operand(right);
			}
		} else {
			raw_string_ostream os(name);
			v->printAsOperand(os, false);
		}
	}
}

Formula Atom::create(Context &c, Operator op, llvm::StringRef lhs, llvm::StringRef rhs) {
	Operand *l = c.get_operand(lhs.str());
	Operand *r = c.get_operand(rhs.str());
	return Formula(new Atom(c, op, l, r));
}

Formula Atom::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	Atom *ret = new Atom(c);

	if (op == OP_NULL) {
		ret->name = name;
	} else {
		ret->op = op;
		ret->lhs = c.get_operand(lhs, sub);
		ret->rhs = c.get_operand(rhs, sub);
	}

	Formula f(ret);
	return f;
}

Formula Conjunction::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	return (p->deep_copy(c, sub) && q->deep_copy(c, sub));
}

Formula Disjunction::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	return (p->deep_copy(c, sub) || q->deep_copy(c, sub));
}

Formula Negation::deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) {
	return (!p->deep_copy(c, sub));
}

Formula operator&&(Formula p, Formula q) {
	if (p.get() == NULL)
		return q;
	if (q.get() == NULL)
		return p;

	assert(p);
	assert(q);
	assert(&p->c == &q->c);
	if (p->is_false() || q->is_false())
		return False::get(p->c);
	if (p->is_true())
		return q;
	if (q->is_true())
		return p;
	return Formula(new Conjunction(p->c, p, q));
}

Formula operator||(Formula p, Formula q) {
	if (p.get() == NULL)
		return q;
	if (q.get() == NULL)
		return p;

	assert(p);
	assert(q);
	assert(&p->c == &q->c);
	if (p->is_true() || q->is_true())
		return True::get(p->c);
	if (p->is_false())
		return q;
	if (q->is_false())
		return p;
	return Formula(new Disjunction(p->c, p, q));
}

Formula operator!(Formula p) {
	if (p.get() == NULL)
		return NULL;

	assert(p);
	if (p->is_true())
		return False::get(p->c);
	if (p->is_false())
		return True::get(p->c);
	return Formula(new Negation(p->c, p));
}

llvm::raw_ostream & operator<<(llvm::raw_ostream & out, Formula const & e) {
	if (e)
		e->print(out);
	else
		out << "<null>";
	return out;
}

};
