#include "Summary.h"

#include <list>
#include <string>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include "util.h"

using namespace llvm;

struct PredefinedParser : public cl::parser<llvm::BitVector> {
	std::list<std::string> keys;

	PredefinedParser() {
		keys.push_back("kref");
		keys.push_back("dpm");
		keys.push_back("ffs");
		keys.push_back("py");
	}

	bool parse(cl::Option &O,
		   StringRef ArgName, const std::string &ArgValue,
		   llvm::BitVector &Val) {
		std::string type;
		int pos = 0;
		Val.resize(keys.size());
		while (pos < ArgValue.size()) {
			int comma = ArgValue.find(',', pos);
			if (comma == std::string::npos)
				comma = ArgValue.size();
			type = ArgValue.substr(pos, comma - pos);
			int bit = 0;
			for (auto &key : keys) {
				if (type == key) {
					Val.set(bit);
					break;
				}
				++bit;
			}
			pos = comma + 1;
		}

		return false;
	}
};

cl::opt<llvm::BitVector, false, PredefinedParser>
PREDEFINED("predefined",
	   cl::init(BitVector(32)),
	   cl::desc("Predefined summaries to be enabled (a comma-separated list)"));

namespace rid {

static std::string scratch;
static llvm::raw_string_ostream os(scratch);

static bool get_no_return(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]:" << type;
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = True::get(s.c);
	Operation &op = entry.ops[scratch];
	op.refcount_sig = scratch;
	op.amount = 1;
	op.host = getFunctionName(&F);

	return true;
}


static bool get_return_success(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]:" << type;
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = True::get(s.c);
	Operation &op = entry.ops[scratch];
	op.refcount_sig = scratch;
	op.amount = 1;
	op.host = getFunctionName(&F);
	entry.ret = "0";

	s.summaries.emplace_back();
	PathSummaryEntry &entry2 = s.summaries.back();
	entry2.pc = Formula(new Atom(s.c, Atom::OP_NE, s.c.get_operand("[0]"), s.c.get_operand(0)));
	entry2.ret = "0";

	return true;
}

static bool get_nonnull_no_return(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]";
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = Formula(new Atom(s.c, Atom::OP_NE, s.c.get_operand(scratch), s.c.get_operand(0)));

	os << ":" << type;
	os.flush();

	Operation &op = entry.ops[scratch];
	op.refcount_sig = scratch;
	op.amount = 1;
	op.host = getFunctionName(&F);

	return true;
}

static bool get_new(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = Formula(new Atom(s.c, Atom::OP_NE, s.c.get_operand("[0]"), s.c.get_operand(0)));
	Operation &op = entry.ops["[0]:" + type];
	op.refcount_sig = "[0]:" + type;
	op.amount = 1;
	op.host = getFunctionName(&F);
	entry.ret = "[0]";

	s.summaries.emplace_back();
	PathSummaryEntry &entry2 = s.summaries.back();
	entry2.pc = Formula(new Atom(s.c, Atom::OP_EQ, s.c.get_operand("[0]"), s.c.get_operand(0)));
	entry2.ret = "0";

	return true;
}

static bool get_unless_zero(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]:" << type;
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = Formula(new Atom(s.c, Atom::OP_GT, s.c.get_operand("[0]"), s.c.get_operand(0)));
	Operation &op = entry.ops[scratch];
	op.refcount_sig = scratch;
	op.amount = 1;
	op.host = getFunctionName(&F);
	entry.ret = "[0]";

	s.summaries.emplace_back();
	PathSummaryEntry &entry2 = s.summaries.back();
	entry2.pc = True::get(s.c);
	entry2.ret = "0";

	return true;
}

static bool put_no_return(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]:" << type;
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = True::get(s.c);
	Operation &op = entry.ops[scratch];
	op.refcount_sig = scratch;
	op.amount = -1;
	op.host = getFunctionName(&F);

	return true;
}

static bool get_return_any(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]:" << type;
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = True::get(s.c);
	Operation &op = entry.ops[scratch];
	op.refcount_sig = scratch;
	op.amount = 1;
	op.host = getFunctionName(&F);
	entry.ret = "[0]";

	return true;
}

static bool put_return_any(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]:" << type;
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = True::get(s.c);
	Operation &op = entry.ops[scratch];
	op.refcount_sig = scratch;
	op.amount = -1;
	op.host = getFunctionName(&F);
	entry.ret = "[0]";

	return true;
}

static bool noop_ret(llvm::Function &F, int target, const std::string &type) {
	Summary &s = summaryBase[&F];
	s.F = &F;

	scratch.clear();
	os << "[" << target << "]";
	os.flush();

	s.summaries.emplace_back();
	PathSummaryEntry &entry = s.summaries.back();
	entry.pc = True::get(s.c);
	entry.ret = scratch;

	return true;
}

static bool noop_noret(llvm::Function &F, int target, const std::string &type) {
	return true;
}

bool has_predefined_summary(llvm::Function &F) {
	StringRef name = getFunctionName(&F);

	// kref
	if (PREDEFINED[0]) {
		if (name.equals("kref_init") ||
		    name.equals("kref_get"))
			return get_no_return(F, 1, "kref");

		if (name.equals("kref_get_unless_zero"))
			return get_unless_zero(F, 1, "kref");

		if (name.equals("kref_put") ||
		    name.equals("kref_put_spinlock_irqsave") ||
		    name.equals("kref_put_mutex"))
			return put_no_return(F, 1, "kref");

		if (name.equals("kobject_get") ||
		    name.equals("kobject_get_unless_zero@kobject"))
			return noop_ret(F, 1, "kref");

		if (name.equals("kobject_init_internal@kobject") ||
		    name.equals("kobject_put"))
			return noop_noret(F, 0, "kref");
	}

	// dpm
	if (PREDEFINED[1]) {
		if (name.equals("pm_runtime_get") ||
		    name.equals("pm_runtime_get_sync") ||
		    name.equals("pm_runtime_get_noresume"))
			return get_return_any(F, 1, "dpm");

		if (name.equals("pm_runtime_put") ||
		    name.equals("pm_runtime_put_noidle") ||
		    name.equals("pm_runtime_put_autosuspend") ||
		    name.equals("pm_runtime_put_sync") ||
		    name.equals("pm_runtime_put_sync_suspend") ||
		    name.equals("pm_runtime_put_sync_autosuspend"))
			return put_return_any(F, 1, "dpm");
	}

	// ffs
	if (PREDEFINED[2]) {
		if (name.equals("ffs_data_new"))
			return get_new(F, 0, "ffs");

		if (name.equals("ffs_data_get"))
			return get_no_return(F, 1, "ffs");

		if (name.equals("ffs_data_put"))
			return put_no_return(F, 1, "ffs");
	}

	// Py/C
	if (PREDEFINED[3]) {
		if (name.equals("_Py_INCREF"))
			return get_no_return(F, 1, "py");
		if (name.equals("PyErr_SetObject"))
			return get_nonnull_no_return(F, 2, "py");
		if (name.equals("PyObject_SetAttrString"))
			return get_return_success(F, 3, "py");
		if (name.equals("_Py_DECREF"))
			return put_no_return(F, 1, "py");
		if (name.equals("Py_BuildValue") ||
		    name.equals("_Py_BuildValue_SizeT") ||
		    name.equals("PyNumber_Long") ||
		    name.equals("PyInt_FromLong") ||
		    name.equals("PyLong_FromLong") ||
		    name.equals("PyLong_FromUnsignedLong") ||
		    name.equals("PyLong_FromUnsignedLongLong") ||
		    name.equals("PyFloat_FromDouble") ||
		    name.equals("PyString_FromString") ||
		    name.equals("PyString_FromStringAndSize") ||
		    name.equals("PyObject_GetAttr") ||
		    name.equals("PyObject_GetAttrString") ||
		    name.equals("PyCFunction_NewEx") ||
		    name.equals("PyMethod_New") ||
		    name.equals("PyDict_New") ||
		    name.equals("PyList_New") ||
		    name.equals("PyTuple_New") ||
		    name.equals("PyDictProxy_New") ||
		    name.equals("PyEval_CallMethod") ||
		    name.equals("PyObject_CallFunctionObjArgs") ||
		    name.equals("PyEval_CallObjectWithKeywords") ||
		    name.equals("PyObject_GetItem") ||
		    name.equals("PySequence_GetItem") ||
		    name.equals("PySequence_GetSlice") ||
		    name.equals("PyMapping_GetItemString") ||
		    name.equals("PyCObject_FromVoidPtrAndDesc") ||
		    name.equals("PyTuple_GetSlice"))
			return get_new(F, 0, "py");
		if (name.equals("PyList_Append"))
			return get_return_success(F, 2, "py");
	}

	return false;
}

};
