//===---- Formula.h - QF_LIA Formulas. --------------------------*- C++ -*-===//

#ifndef FORMULA_H
#define FORMULA_H

#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <fstream>
#include <functional>

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/raw_ostream.h>

#include <z3.h>
#include <z3++.h>

namespace rsc {

class Expr;
class Operand;
class Constant;
class Variable;
class Signature;
class __Formula;
typedef std::shared_ptr<__Formula> Formula;

class Context {
	/* Operand pool */
	std::vector<Operand*> operands;                    // Owner of constants and signatures

	Constant *get_constant(long long c);
	Variable *get_variable(llvm::Value *v);
	Variable *get_variable(const std::string &name);
	Signature *get_signature(const std::string &sig);

	Operand *parse_to_operand(Z3_ast ast);
	Formula parse_to_formula(Z3_ast ast);

public:
	z3::context z3;
	llvm::Function *F;

	// maps for operands
	std::map<int, Constant*> constants;
	std::map<llvm::Value*, Variable*> variables;       // Owner of variables
	std::map<std::string, Variable*> name_to_variables;
	std::map<std::string, Signature*> signatures;

	void operand_is_legal(Operand *op);

	// maps for atoms
	std::map<llvm::Value*, Formula> value_to_atoms;
	std::map<std::string, Formula> name_to_atoms;      // Atoms of type bool_const

	int pathid;
	std::map<int, int> pathtree;                       // new path -> old path

	Context() : pathid(0) { pathtree[0] = -1; }
	~Context();

	Operand *get_operand(llvm::Value *v);
	Operand *get_operand(int i) { return get_operand((long long)i); }
	Operand *get_operand(long long i);
	Operand *get_operand(const char *sig);
	Operand *get_operand(const std::string &sig);
	Operand *get_operand(Operand *op, std::function<Expr*(Context&, Expr*)> sub);

	Formula get_atom(llvm::Value *v);
	Formula get_atom(const std::string &name);

	Formula parse(z3::expr e);

	void switch_pathid(int id) { pathid = id; }
	void copy_path(int old_id, int new_id) { pathtree[new_id] = old_id; }

	void dump_var_bindings();
};

class Expr {
public:
	Context &c;

protected:
	virtual void init_expr() { assert(0); };

public:
	Expr(Context &c) : c(c) {}
	virtual ~Expr() {}

	virtual z3::expr z3_expr() = 0;

	virtual void serialize(std::ofstream &fout) = 0;
};

class Operand : public Expr {
public:
	Operand(Context &c) : Expr(c) {}
	virtual ~Operand() {}

	virtual bool is_constant() { return false; }
	virtual bool is_variable() { return false; }
	virtual bool is_signature() { return false; }

	virtual Operand *deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) = 0;

	static Operand *deserialize(Context &c, std::ifstream &fin);

	virtual void print(llvm::raw_ostream & out) = 0;
};

class Constant : public Operand {
public:
	long long i;

	Constant(Context &c) : Operand(c) {}
	virtual ~Constant() {}

	virtual bool is_constant() { return true; }

	virtual z3::expr z3_expr() {
		return c.z3.int_val(i);
	}

	virtual Operand *deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Operand *deserialize(Context &c, std::ifstream &fin);

	virtual void print(llvm::raw_ostream & out) {
		if (i < 10)
			out << i;
		else
			out << llvm::format("0x%x", i);
	}
};

class Variable : public Operand {
public:
	std::string name;
	llvm::Value *v;

	Variable(Context &c) : Operand(c), v(NULL) {}
	virtual ~Variable() {}

	virtual bool is_variable() { return true; }

	virtual z3::expr z3_expr() {
		return c.z3.int_const(name.c_str());
	}

	virtual Operand *deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);

	virtual void print(llvm::raw_ostream & out) {
		if (v)
			v->printAsOperand(out, false);
		else if (!name.empty())
			out << name;
		else
			out << "??";
	}
};

class Signature : public Operand {
public:
	std::string sig;

	Signature(Context &c) : Operand(c) {}
	virtual ~Signature() {}

	virtual bool is_signature() { return true; }

	virtual z3::expr z3_expr() {
		return c.z3.int_const(sig.c_str());
	}

	virtual Operand *deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Operand *deserialize(Context &c, std::ifstream &fin);

	virtual void print(llvm::raw_ostream & out) {
		if (!sig.empty())
			out << sig;
		else
			out << "??";
	}
};

class __Formula : public Expr {
	friend class FormulaVisitor;
protected:

	enum Type {
		T_Base,
		T_True,
		T_False,
		T_Atom,
		T_Conjunction,
		T_Disjunction,
		T_Negation,
	};

	__Formula(Context &c) : Expr(c) {}

	virtual Type get_type() { return T_Base; };

public:
	virtual ~__Formula() {}

	Formula simplify();
	Formula deep_simplify();
	bool check();

	bool is_true() { return get_type() == T_True; }
	bool is_false() { return get_type() == T_False; }

	virtual Formula deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub) = 0;

	static Formula deserialize(Context &c, std::ifstream &fin);

	virtual void print(llvm::raw_ostream & out) = 0;

	friend llvm::raw_ostream & operator<<(llvm::raw_ostream & out, Formula const & e);
};

class True : public __Formula {
	True(Context &c) : __Formula(c) {}

protected:
	virtual Type get_type() { return T_True; }

public:
	virtual ~True() {}

	static Formula get(Context &c) {
		Formula t(new True(c));
		return t;
	}

	virtual z3::expr z3_expr() {
		return c.z3.bool_val(true);
	}

	virtual Formula deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Formula deserialize(Context &c, std::ifstream &fin);

	virtual void print(llvm::raw_ostream & out) {
		out << "True";
	}
};

class False : public __Formula {
	False(Context &c) : __Formula(c) {}

protected:
	virtual Type get_type() { return T_False; }

public:
	virtual ~False() {}

	static Formula get(Context &c) {
		Formula f(new False(c));
		return f;
	}

	virtual z3::expr z3_expr() {
		return c.z3.bool_val(false);
	}

	virtual Formula deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Formula deserialize(Context &c, std::ifstream &fin);

	virtual void print(llvm::raw_ostream & out) {
		out << "False";
	}
};

class Atom : public __Formula {
public:
	enum Operator {
		OP_NULL,
		OP_BEGIN,
		OP_EQ = OP_BEGIN,
		OP_NE,
		OP_LT,
		OP_LE,
		OP_GT,
		OP_GE,
		OP_END,
	};

	static const char *OP_SYMBOL[OP_END];

protected:
	virtual Type get_type() { return T_Atom; }

public:
	Operator op;
	Operand *lhs, *rhs;

	llvm::Value *v;
	std::string name;

	virtual ~Atom() {}

	Atom(Context &c) : __Formula(c), op(OP_NULL), lhs(NULL), rhs(NULL) {}
	Atom(Context &c, llvm::Value *v);
	Atom(Context &c, const char *name) : __Formula(c), op(OP_NULL), lhs(NULL), rhs(NULL), name(name) {}
	Atom(Context &c, Operator op, Operand *lhs, Operand *rhs)
		: __Formula(c), v(NULL), op(op), lhs(lhs), rhs(rhs) {}

	static Formula create(Context &c, Operator op, llvm::StringRef lhs, llvm::StringRef rhs);

	llvm::StringRef getName() { return llvm::StringRef(name); }

	virtual z3::expr z3_expr() {
		switch (op) {
		case OP_EQ:
			return (lhs->z3_expr() == rhs->z3_expr());
		case OP_NE:
			return (lhs->z3_expr() != rhs->z3_expr());
		case OP_LT:
			return (lhs->z3_expr() < rhs->z3_expr());
		case OP_LE:
			return (lhs->z3_expr() <= rhs->z3_expr());
		case OP_GT:
			return (lhs->z3_expr() > rhs->z3_expr());
		case OP_GE:
			return (lhs->z3_expr() >= rhs->z3_expr());
		}
		return c.z3.bool_const(name.c_str());
	}

	Formula deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Formula deserialize(Operator op, Context &c, std::ifstream &fin);

	virtual void print(llvm::raw_ostream & out) {
		if (op == OP_NULL)
			out << name;
		else {
			out << "(";
			lhs->print(out);
			out << " " << OP_SYMBOL[op] << " ";
			rhs->print(out);
			out << ")";
		}
	}
};

class Conjunction : public __Formula {
	Conjunction(Context &c, Formula p, Formula q)
		: __Formula(c), p(p), q(q) {}

protected:
	virtual Type get_type() { return T_Conjunction; }
public:
	Formula p, q;

	virtual ~Conjunction() {}

	virtual z3::expr z3_expr() {
		return (p->z3_expr() && q->z3_expr());
	}

	Formula deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Formula deserialize(Context &c, std::ifstream &fin);

	friend Formula operator&&(Formula p, Formula q);

	virtual void print(llvm::raw_ostream & out) {
		out << "(";
		p->print(out);
		out << " /\\ ";
		q->print(out);
		out << ")";
	}
};

class Disjunction : public __Formula {
	Disjunction(Context &c, Formula p, Formula q)
		: __Formula(c), p(p), q(q) {}

protected:
	virtual Type get_type() { return T_Disjunction; }
public:
	Formula p, q;

	virtual ~Disjunction() {}

	virtual z3::expr z3_expr() {
		return (p->z3_expr() || q->z3_expr());
	}

	Formula deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Formula deserialize(Context &c, std::ifstream &fin);

	friend Formula operator||(Formula p, Formula q);

	virtual void print(llvm::raw_ostream & out) {
		out << "(";
		p->print(out);
		out << " \\/ ";
		q->print(out);
		out << ")";
	}
};

class Negation : public __Formula {
	Negation(Context &c, Formula p)
		: __Formula(c), p(p) {}

protected:
	virtual Type get_type() { return T_Negation; }
public:
	Formula p;

	virtual ~Negation() {}

	virtual z3::expr z3_expr() {
		return !p->z3_expr();
	}

	Formula deep_copy(Context &c, std::function<Expr*(Context&, Expr*)> sub);

	virtual void serialize(std::ofstream &fout);
	static Formula deserialize(Context &c, std::ifstream &fin);

	friend Formula operator!(Formula p);

	virtual void print(llvm::raw_ostream & out) {
		out << "~";
		p->print(out);
	}
};

Formula operator&&(Formula p, Formula q);
Formula operator||(Formula p, Formula q);
Formula operator!(Formula p);
llvm::raw_ostream & operator<<(llvm::raw_ostream & out, Formula const & e);

}

#endif  /* FORMULA_H */
