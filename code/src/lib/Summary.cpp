#include "Summary.h"

#include <fstream>
#include <iostream>
#include <map>
#include <cmath>

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include "util.h"

using namespace llvm;

namespace rsc {

unsigned int Operation::next_id = 0;

SummaryBase summaryBase;

MultipathSignatureData& SignatureMap::operator[](llvm::Value *v) {
	try {
		return this->at(v);
	} catch (std::out_of_range &e) {}

	MultipathSignatureData &ret = (*((this->insert(std::make_pair(v,MultipathSignatureData(c)))).first)).second;
	std::string buf;
	llvm::raw_string_ostream os(buf);

	Value *target = v;

again:
	if (auto ci = dyn_cast<ConstantInt>(target)) {
		os << ci->getSExtValue();
	} else if (auto cpn = dyn_cast<ConstantPointerNull>(target)) {
		os << 0;
	} else if (dyn_cast<GlobalVariable>(target) != NULL) {
		os << "[" << target->getName() << "]";
	} else if (auto expr = dyn_cast<ConstantExpr>(target)) {
		auto *i = expr->getAsInstruction();
		if (auto gep = dyn_cast<GetElementPtrInst>(i)) {
			Value *base = gep->getPointerOperand();
			std::list<std::string> comp;
			compose_get_element_ptr_sig(*gep, comp);
			comp.push_front((*this)[base].get());
			os << compose_sig(comp, ".");
		} else if (expr->isCast()) {
			target = expr->getOperand(0);
			delete i;
			goto again;
		} else {
			// fall back to the default form
			os << "<";
			target->printAsOperand(os, false);
			os << ">";
		}
		delete i;
	} else {
		// We use intermediate representations of variables here so that
		// they will be kept as variables in formulas when no further
		// signatures are found for them. They must be converted to the
		// local variable form (i.e. {xxx}) once the end of a path is reached.
		os << "<";
		target->printAsOperand(os, false);
		os << ">";
	}
	os.flush();
	ret.init(buf);

	return ret;
}

void Operation::add_history_entry(Operation *op) {
	for (auto i = from.begin(), e = from.end(); i != e; ++i) {
		if (*i == op)
			return;
		if ((*i)->host.compare(op->host) > 0) {
			from.insert(i, op);
			return;
		}
	}
	from.push_back(op);
}

void Operation::print(llvm::raw_ostream &os, const std::string &prefix) {
	os << prefix << refcount_sig << " ";
	if (amount > 0)
		os << "+";
	os << amount << "\n";
	print_history(os, prefix);
}

void Operation::print_history(llvm::raw_ostream &os, const std::string &prefix) {
	int i = 0;
	for (auto entry : from) {
		++i;
		if (i == from.size())
			os << prefix << "`-- ";
		else
			os << prefix << "+-- ";
		os << entry->host;
		if (!entry->refcount_sig.empty())
			os << " " << entry->refcount_sig;
		os << " ";
		if (entry->amount > 0)
			os << "+";
		os << entry->amount << "\n";

		if (prefix.size() < 4 * 16) {
			if (i == from.size())
				entry->print_history(os, prefix + "    ");
			else
				entry->print_history(os, prefix + "|   ");
		}
	}
}

bool RefcountOps::diff(RefcountOps &rhs, std::set<std::pair<std::string, int>> &tainted) {
	bool ret = false;

	for (auto &pair : *this) {
		const RefcountSig &sig = pair.first;
		Operation &op = pair.second;
		if (op.amount == 0)
			continue;
		bool same = false;
		int delta = 0;
		try {
			delta = std::abs(op.amount - rhs.at(sig).amount);
			if (delta == 0)
				same = true;
		} catch (std::out_of_range &e) {
			delta = std::abs(op.amount);
		}
		if (!same) {
			tainted.insert(std::make_pair(sig, delta));
			ret = true;
		}
	}

	for (auto &pair : rhs) {
		const RefcountSig &sig = pair.first;
		Operation &op = pair.second;
		if (op.amount == 0)
			continue;
		bool same = false;
		int delta = 0;
		try {
			delta = std::abs(op.amount - this->at(sig).amount);
			if (delta == 0)
				same = true;
		} catch (std::out_of_range &e) {
			delta = std::abs(op.amount);
		}
		if (!same) {
			tainted.insert(std::make_pair(sig, delta));
			ret = true;
		}
	}

	return ret;
}

bool RefcountOps::operator==(RefcountOps &rhs) {
	for (auto &pair : *this) {
		const RefcountSig &sig = pair.first;
		Operation &op = pair.second;
		if (op.amount == 0)
			continue;
		try {
			if (op.amount != rhs.at(sig).amount)
				return false;
		} catch (std::out_of_range &e) {
			return false;
		}
	}

	for (auto &pair : rhs) {
		const RefcountSig &sig = pair.first;
		Operation &op = pair.second;
		if (op.amount == 0)
			continue;
		try {
			if (op.amount != this->at(sig).amount)
				return false;
		} catch (std::out_of_range &e) {
			return false;
		}
	}

	return true;
}

bool RefcountOps::is_pure() {
	for (auto &pair : *this)
		if (pair.second.amount != 0)
			return false;
	return true;
}

bool RefcountOps::print(llvm::raw_ostream &os, const std::string &prefix, RefcountOps *otherpath) {
	bool ret = false;
	for (auto &pair : *this) {
		const RefcountSig &sig = pair.first;
		Operation &op = pair.second;

		if (op.amount == 0)
			continue;

		bool same = false;
		if (otherpath) {
			try {
				if (op.amount == otherpath->at(sig).amount)
					same = true;
			} catch (std::out_of_range &e) {}
		}
		if (!same) {
			op.print(os, prefix);
			ret = true;
		}
	}
	return ret;
}

void PathSummaryEntry::print(
	llvm::raw_ostream &os, PathPrintType pt,
	path_iterator *pi, PathSummaryEntry *otherpath,
	const std::string &prefix, const std::string &delimiter) {
	if (pt == EXACT && exact_pc.get() != NULL) {
		os << prefix << exact_pc << "\n";
	} else if (pt == BBPATH && !path.empty() && pi) {
		os << prefix;
		pi->pretty_print_path(os, path);
		os << "\n";
	} else {
		os << prefix << pc << "\n";
	}
	if (!ops.print(os, prefix + delimiter, otherpath ? &otherpath->ops : NULL) && ret.empty())
		os << prefix << delimiter << "-\n";
	if (!ret.empty())
		os << prefix << delimiter << "returns " << ret << "\n";
}

bool Summary::print(llvm::raw_ostream &os, bool complete) {
	if (summaries.empty())
	    return false;

	if (F)
		os << F->getName() << " (" << getInitialName(F) << "@" << getLocation(F) << ")" << "\n";
	else if (!name.empty())
		os << name << "\n";
	for (auto &s : summaries)
		s.print(os);
	if (complete && !dropped_summaries.empty()) {
		os << "\t~~~~~ Summaries below are dropped due to inconsistency ~~~~~\n";
		for (auto &s : dropped_summaries)
			s.print(os);
	}

	return true;
}

boost::regex InstantiatedSummary::container_elim_l(".-([\\w]+).\\1");
boost::regex InstantiatedSummary::container_elim_r(".([\\w]+).-\\1");

void InstantiatedSummary::formal_to_actual(
	VariableSignature &sig,
	llvm::CallInst *ci,
	SignatureMap &sigmap) {
	char arg_name[4];
	strcpy(arg_name, "[0]");

	replace_all(sig, arg_name, sigmap[ci].get());
	for (int i = 0, e = ci->getNumArgOperands(); i < e; ++i) {
		arg_name[1] = '0' + i + 1;
		replace_all(sig, arg_name, sigmap[ci->getArgOperand(i)].get());
	}

	sig = boost::regex_replace(sig, container_elim_l, "");
	sig = boost::regex_replace(sig, container_elim_r, "");
}

InstantiatedSummary::InstantiatedSummary(
	Summary &parameterized,
	Context &c,
	llvm::CallInst *ci,
	SignatureMap &sigmap)
	: c(c) {
	// If the return value or any actual argument is a local variable,
	// create the corresponding Variable operand in the context in case it
	// is needed in path conditions.
	c.get_operand(ci);
	for (int i = 0, e = ci->getNumArgOperands(); i < e; ++i)
		c.get_operand(ci->getArgOperand(i));

	for (auto &entry : parameterized.summaries) {
		summaries.emplace_back();
		auto &inst_entry = summaries.back();
		/* Instantiate the path condition */
		inst_entry.pc = entry.pc->deep_copy(c, [&](Context &c, Expr *e) -> Expr* {
				if (auto s = dynamic_cast<Signature*>(e)) {
					std::string sig = s->sig;
					formal_to_actual(sig, ci, sigmap);
					return c.get_operand(sig);
				}
				return NULL;
			});

		/* Instantiate refcount signatures */
		for (auto &pair : entry.ops) {
			RefcountSig sig = pair.first;
			Operation &op = pair.second;
			if (*sig.begin() == '{' && sig.find("}:") != std::string::npos)
				continue;
			if (op.amount == 0)
				continue;
			formal_to_actual(sig, ci, sigmap);
			inst_entry.ops[sig] = &op;
		}

		/* Instantiate the return values */
		inst_entry.ret = entry.ret;
		formal_to_actual(inst_entry.ret, ci, sigmap);
	}
}

bool InstantiatedSummary::print(llvm::raw_ostream &os) {
	if (summaries.empty())
	    return false;

	for (auto &s : summaries) {
		os << "\t" << s.pc << "\n";
		for (auto &pair : s.ops) {
			const RefcountSig &sig = pair.first;
			Operation *op = pair.second;
			os << "\t\t" << sig << ": ";
			if (op->amount > 0)      os << "+";
			os << op->amount << "\n";
		}
		if (s.ops.empty())
			os << "\t\t-\n";
		if (!s.ret.empty())
			os << "\t\treturns " << s.ret << "\n";
	}

	return true;
}

bool Summary::is_pure() {
	if (pure == UNKNOWN) {
		pure = PURE;
		for (auto s : summaries)
			if (!s.ops.is_pure()) {
				pure = IMPURE;
				break;
			}
	}
	return pure == PURE;
}

bool isPure(llvm::Function &F) {
	bool pure;
	try {
		pure = summaryBase.at(&F).is_pure();
	} catch (std::out_of_range &e) {
		pure = true;
	}
	return pure;
}

};
