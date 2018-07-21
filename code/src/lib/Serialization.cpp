#include "Formula.h"
#include "Summary.h"

#include <cassert>

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include "util.h"

using namespace llvm;

cl::opt<std::string>
O_CACHE("o-cache",
	cl::init(""),
	cl::desc("Serialize function summaries to the file"));

cl::opt<std::string>
I_CACHE("i-cache",
	cl::init(""),
	cl::desc("Deserialize function summaries from the file"));

namespace rsc {

static std::ifstream fin;
static std::ofstream fout;

std::list<std::string> fns;
static std::map<std::string, long> name2pos;
static std::map<int, Operation *> id2ops;

/******************************************************************************
 * Common helpers
 ******************************************************************************/

enum OperandType {
	OP_NULL,
	OP_CONSTANT,
	OP_SIGNATURE,
};

enum FormulaType {
	F_NULL,
	F_TRUE,
	F_FALSE,
	F_CONJUNCTION,
	F_DISJUNCTION,
	F_NEGATION,
	F_EQ,
	F_NE,
	F_LT,
	F_LE,
	F_GT,
	F_GE,
};

static void serialize_int(std::ofstream &fout, const int i) {
	fout.write(reinterpret_cast<const char*>(&i), sizeof(i));
}

static void serialize_uint(std::ofstream &fout, const unsigned int i) {
	fout.write(reinterpret_cast<const char*>(&i), sizeof(i));
}

static void serialize_operand_type(std::ofstream &fout, const OperandType t) {
	serialize_uint(fout, static_cast<const unsigned int>(t));
}

static void serialize_formula_type(std::ofstream &fout, const FormulaType t) {
	serialize_uint(fout, static_cast<const unsigned int>(t));
}

static void serialize_string(std::ofstream &fout, const std::string &str) {
	unsigned int length = str.size() + 1;
	serialize_uint(fout, length);
	fout.write(str.c_str(), length);
}

static int deserialize_int(std::ifstream &fin) {
	int i;
	fin.read(reinterpret_cast<char *>(&i), sizeof(i));
	return i;
}

static unsigned int deserialize_uint(std::ifstream &fin) {
	unsigned int i;
	fin.read(reinterpret_cast<char *>(&i), sizeof(i));
	return i;
}

static OperandType deserialize_operand_type(std::ifstream &fin) {
	unsigned int i = deserialize_uint(fin);
	return static_cast<OperandType>(i);
}

static FormulaType deserialize_formula_type(std::ifstream &fin) {
	unsigned int i = deserialize_uint(fin);
	return static_cast<FormulaType>(i);
}

static StringRef deserialize_string(std::ifstream &fin) {
	static char buf[1024];

	unsigned int length = deserialize_uint(fin);
	assert(length < 1024);
	fin.read(buf, length);
	return StringRef(buf);
}

/******************************************************************************
 * Serialization
 ******************************************************************************/

void Constant::serialize(std::ofstream &fout) {
	serialize_operand_type(fout, OP_CONSTANT);
	serialize_int(fout, i);
}

void Variable::serialize(std::ofstream &fout) {
	assert(0 && "Variables cannot be serialized!");
}

void Signature::serialize(std::ofstream &fout) {
	serialize_operand_type(fout, OP_SIGNATURE);
	serialize_string(fout, sig);
}

void True::serialize(std::ofstream &fout) {
	serialize_formula_type(fout, F_TRUE);
}

void False::serialize(std::ofstream &fout) {
	serialize_formula_type(fout, F_FALSE);
}

void Atom::serialize(std::ofstream &fout) {
	assert(lhs);
	assert(rhs);

	switch(op) {
	case OP_EQ: serialize_formula_type(fout, F_EQ); break;
	case OP_NE: serialize_formula_type(fout, F_NE); break;
	case OP_LT: serialize_formula_type(fout, F_LT); break;
	case OP_LE: serialize_formula_type(fout, F_LE); break;
	case OP_GT: serialize_formula_type(fout, F_GT); break;
	case OP_GE: serialize_formula_type(fout, F_GE); break;
	default:
		assert(0 && "Unknown Atom operation type!");
	}

	lhs->serialize(fout);
	rhs->serialize(fout);
}

void Conjunction::serialize(std::ofstream &fout) {
	serialize_formula_type(fout, F_CONJUNCTION);
	p->serialize(fout);
	q->serialize(fout);
}

void Disjunction::serialize(std::ofstream &fout) {
	serialize_formula_type(fout, F_DISJUNCTION);
	p->serialize(fout);
	q->serialize(fout);
}

void Negation::serialize(std::ofstream &fout) {
	serialize_formula_type(fout, F_NEGATION);
	p->serialize(fout);
}

void Operation::serialize(std::ofstream &fout) {
	serialize_int(fout, amount);
	serialize_uint(fout, id);
	serialize_uint(fout, from.size());
	for (auto op : from)
		serialize_uint(fout, op->id);
}

void RefcountOps::serialize(std::ofstream &fout) {
	serialize_uint(fout, size());
	for (auto &pair : (*this)) {
		const RefcountSig &sig = pair.first;
		Operation &op = pair.second;
		serialize_string(fout, sig);
		op.serialize(fout);
	}
}

void PathSummaryEntry::serialize(std::ofstream &fout) {
	pc->serialize(fout);
	ops.serialize(fout);
	serialize_string(fout, ret);
}

void Summary::serialize(std::ofstream &fout) {
	if (F)
		serialize_string(fout, getFunctionName(F).str());
	else if (!name.empty())
		serialize_string(fout, name);
	else
		assert(0 && "Summary has no name!");

	unsigned int length = 0;

	long length_pos = fout.tellp();
	serialize_uint(fout, length);    // placeholder

	long start_pos = fout.tellp();
	serialize_uint(fout, summaries.size());
	for (auto &entry : summaries)
		entry.serialize(fout);
	serialize_uint(fout, dropped_summaries.size());
	for (auto &entry : dropped_summaries)
		entry.serialize(fout);
	long end_pos = fout.tellp();

	fout.seekp(length_pos);
	length = end_pos - start_pos;
	serialize_uint(fout, length);

	fout.seekp(0, std::ios_base::end);
}

/******************************************************************************
 * Deserialization
 ******************************************************************************/

Operand *Operand::deserialize(Context &c, std::ifstream &fin) {
	OperandType type = deserialize_operand_type(fin);
	switch (type) {
	case OP_CONSTANT:  return Constant::deserialize(c, fin);
	case OP_SIGNATURE: return Signature::deserialize(c, fin);
	}
	assert(0 && "Unknown operand type!");
}

Operand *Constant::deserialize(Context &c, std::ifstream &fin) {
	/* Must be called from Operand::deserialize() */
	int i = deserialize_int(fin);
	return c.get_operand(i);
}

Operand *Signature::deserialize(Context &c, std::ifstream &fin) {
	/* Must be called from Operand::deserialize() */
	StringRef s = deserialize_string(fin);
	return c.get_operand(s.str());
}

Formula __Formula::deserialize(Context &c, std::ifstream &fin) {
	FormulaType type = deserialize_formula_type(fin);
	switch (type) {
	case F_TRUE:        return True::deserialize(c, fin);
	case F_FALSE:       return False::deserialize(c, fin);
	case F_CONJUNCTION: return Conjunction::deserialize(c, fin);
	case F_DISJUNCTION: return Disjunction::deserialize(c, fin);
	case F_NEGATION:    return Negation::deserialize(c, fin);
	case F_EQ:          return Atom::deserialize(Atom::OP_EQ, c, fin);
	case F_NE:          return Atom::deserialize(Atom::OP_NE, c, fin);
	case F_LT:          return Atom::deserialize(Atom::OP_LT, c, fin);
	case F_LE:          return Atom::deserialize(Atom::OP_LE, c, fin);
	case F_GT:          return Atom::deserialize(Atom::OP_GT, c, fin);
	case F_GE:          return Atom::deserialize(Atom::OP_GE, c, fin);
	}
	assert(0 && "Unknown formula type!");
}

Formula True::deserialize(Context &c, std::ifstream &fin) {
	return True::get(c);
}

Formula False::deserialize(Context &c, std::ifstream &fin) {
	return False::get(c);
}

Formula Atom::deserialize(Operator op, Context &c, std::ifstream &fin) {
	Operand *lhs = Operand::deserialize(c, fin);
	Operand *rhs = Operand::deserialize(c, fin);
	return Formula(new Atom(c, op, lhs, rhs));
}

Formula Conjunction::deserialize(Context &c, std::ifstream &fin) {
	Formula p = __Formula::deserialize(c, fin);
	Formula q = __Formula::deserialize(c, fin);
	return (p && q);
}

Formula Disjunction::deserialize(Context &c, std::ifstream &fin) {
	Formula p = __Formula::deserialize(c, fin);
	Formula q = __Formula::deserialize(c, fin);
	return (p || q);
}

Formula Negation::deserialize(Context &c, std::ifstream &fin) {
	Formula p = __Formula::deserialize(c, fin);
	return (!p);
}

void Operation::deserialize(std::ifstream &fin) {
	amount = deserialize_int(fin);
	id = deserialize_uint(fin);
	id2ops[id] = this;

	unsigned int total = deserialize_uint(fin);
	for (int i = 0; i < total; ++i) {
		unsigned int from_id = deserialize_uint(fin);
		try {
			from.push_back(id2ops.at(from_id));
		} catch (std::out_of_range &e) {}
	}
}

void RefcountOps::deserialize(std::ifstream &fin) {
	unsigned int total = deserialize_uint(fin);
	for (int i = 0; i < total; ++i) {
		RefcountSig sig = deserialize_string(fin).str();
		(*this)[sig].deserialize(fin);
		(*this)[sig].refcount_sig = sig;
	}
}

void PathSummaryEntry::deserialize(std::ifstream &fin, Context &c) {
	pc = __Formula::deserialize(c, fin);
	ops.deserialize(fin);
	ret = deserialize_string(fin).str();
}

void Summary::deserialize(std::ifstream &fin) {
	int total = deserialize_uint(fin);
	for (int i = 0; i < total; ++i) {
		summaries.emplace_back();
		summaries.back().deserialize(fin, c);
	}

	int dropped_total = deserialize_uint(fin);
	for (int i = 0; i < dropped_total; ++i) {
		dropped_summaries.emplace_back();
		dropped_summaries.back().deserialize(fin, c);
	}
}

/******************************************************************************
 * External APIs of caching
 ******************************************************************************/

void cache_open_fin(const std::string &name) {
	if (fin.is_open())
		fin.close();

	fns.clear();
	name2pos.clear();
	id2ops.clear();

	fin.open(name, std::ofstream::in | std::ofstream::binary);
	if (fin.is_open()) {
		std::string name;
		unsigned int length;

		fin.seekg(0, std::ios_base::end);
		long eof = fin.tellg();
		fin.seekg(0, std::ios_base::beg);

		while (fin.tellg() < eof) {
			name = deserialize_string(fin).str();
			length = deserialize_uint(fin);
			name2pos[name] = fin.tellg();
			fns.push_back(name);
			fin.seekg(length, std::ios_base::cur);
		}
	}
}

void cache_open_fout(const std::string &name) {
	fout.open(name, std::ofstream::out | std::ofstream::binary);
}

void cache_init() {
	cache_open_fin(I_CACHE);
	cache_open_fout(O_CACHE);
}

void cache_finalize() {
	if (fin.is_open())
		fin.close();
	if (fout.is_open())
		fout.close();
}

bool serialize_summary(llvm::Function &F) {
	if (!fout.is_open())
		return false;

	try {
		summaryBase.at(&F).serialize(fout);
	} catch (std::out_of_range &e) {
		return false;
	}

	return true;
}

bool serialize_summary(Summary &s, llvm::StringRef fn) {
	if (!fout.is_open())
		return false;

	s.serialize(fout);

	return true;
}

bool deserialize_summary(llvm::Function &F) {
	if (!fin.is_open())
		return false;

	StringRef name = getFunctionName(&F);
	int pos;
	try {
		pos = name2pos.at(name.str());
	} catch (std::out_of_range &e) {
		return false;
	}
	fin.seekg(pos);

	Summary &s = summaryBase[&F];
	s.F = &F;
	s.name = name;
	s.deserialize(fin);

	// fill in the @host fields of operations
	for (auto &entry : s) {
		for (auto &pair : entry.ops) {
			pair.second.host = name;
		}
	}

	return true;
}

bool deserialize_summary(Summary &s, llvm::StringRef fn) {
	if (!fin.is_open())
		return false;

	int pos;
	try {
		pos = name2pos.at(fn.str());
	} catch (std::out_of_range &e) {
		return false;
	}
	fin.seekg(pos);

	s.deserialize(fin);
	s.name = fn.str();

	// fill in the @host fields of operations
	for (auto &entry : s) {
		for (auto &pair : entry.ops) {
			pair.second.host = fn.str();
		}
	}

	return true;
}

};
