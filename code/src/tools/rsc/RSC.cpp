#include <cstdio>
#include <iostream>
#include <cstring>
#include <string>

#include <list>

#include <boost/regex.hpp>

#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/CallGraphSCCPass.h>
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TimeValue.h>

#include "util.h"

using namespace llvm;
//using namespace rsc;

 static cl::opt<int>
MaxPath("max-path-per-func",
	cl::init(100),
	cl::desc("Maximum number of paths to be enumerated in a function"));

static cl::opt<int>
MaxSubcase("max-subcase-per-path",
	   cl::init(10),
	   cl::desc("Maximum number of subcases to be enumerated in a path"));

static cl::opt<bool>
O_TEST("o-test",
       cl::init(false),
       cl::desc("Print final summaries for test"));

static cl::opt<bool>
O_PROGRESS("o-progress",
	   cl::init(false),
	   cl::desc("Print progress to stdout"));

static cl::opt<bool>
O_VERBOSE("o-verbose",
	   cl::init(false),
	   cl::desc("Print internal warnings"));

static cl::opt<std::string>
SINGLE_FN("single-fn",
	  cl::init(""),
	  cl::desc("Verbosely test the analysis of the given function"));

static cl::opt<std::string>
BLACKLIST("blacklist",
	  cl::init(""),
	  cl::desc("A list of function names which are excluded from the analysis"));

static cl::opt<std::string>
PLOT_CFG("plot-cfg",
	 cl::init(""),
	 cl::desc("plot the CFG when an inconsistent path pair is found, saving the files under the given directory"));

cl::opt<std::string>
SENSILIST("sensilist",
	  cl::init(""),
	  cl::desc("A list of functions that should be analyzed"));

/* struct PathTypeParser : public cl::parser<PathSummaryEntry::PathPrintType> {
	bool parse(cl::Option &O,
		   StringRef ArgName, const std::string &ArgValue,
		   PathSummaryEntry::PathPrintType &Val) {
		if (ArgValue == "exact")
			Val = PathSummaryEntry::EXACT;
		else if (ArgValue == "normal")
			Val = PathSummaryEntry::NORMAL;
		else if (ArgValue == "bbpath")
			Val = PathSummaryEntry::BBPATH;

		return false;
	}
};

static cl::opt<PathSummaryEntry::PathPrintType, false, PathTypeParser>
PATHTYPE("path-type",
	 cl::init(PathSummaryEntry::EXACT),
	 cl::desc("Set the form of printed paths"));

#include "PathExecutor.cpp" */


class RSC : public CallGraphSCCPass {

	int progress, total;
	std::string progress_buf;
	raw_string_ostream progress_os;

	bool single_fn_mode;

	std::list<std::string> blacklist;
	std::list<std::string> sensilist;

	int ipp_id;

	/*bool run_on_path(Function &F, Context &c, PathSummary &path_summary, path_iterator &path, int id) {
		if (single_fn_mode) {
			auto edges = path.get_path();
			Formula pc = path.path_condition();

			errs() << "Path: ";
			path.get_entry()->printAsOperand(errs(), false);
			for (auto e : edges) {
				if (!e)
					continue;
				errs() << " ";
				e->target->bb->printAsOperand(errs(), false);
			}
			errs() << "\n";
			errs() << "Path condition: " << pc << "\n";
		}

		PathExecutor exe(F, c, path, path_summary, single_fn_mode);
		exe.run();

		if (single_fn_mode) {
			errs() << "--- Path " << id << " of " << getFunctionName(&F) << "\n";
			for (auto &s : path_summary) {
				errs() << "\tsubcase " << s.id << "\n";
				s.print(errs());
			}
		}

		return true;
	}*/

	/* void merge_path(Summary &summary, PathSummary &path_summary, llvm::Function &F, path_iterator &i) {
		int tainted_size = summary.tainted.size();

		for (auto &entry : path_summary) {
			bool warn = false, merged = false;
			if (!entry.pc->check())
				continue;
			for (auto &s : summary) {
				bool has_field = (s.ret.find('.') != std::string::npos || entry.ret.find('.') != std::string::npos);
				bool same_ops = (s.ops == entry.ops);
				if (same_ops && s.ret == entry.ret) {
					s.pc = s.pc || entry.pc;
					s.exact_pc = s.exact_pc || entry.exact_pc;
					merged = true;
					break;
				}
				if (same_ops)
					continue;
				if (s.ret != entry.ret) {
					if (has_field)
						continue;
					if ((*s.ret.begin() == '[' && *s.ret.rbegin() == ']') ||
					    (*entry.ret.begin() == '[' && *entry.ret.rbegin() == ']'))
						continue;
				}
				Formula cond = s.pc && entry.pc;
				if (!has_field && s.ret != "" && entry.ret != "" && s.ret != entry.ret) {
					bool add_assumption = true;

					if (add_assumption)
						cond = cond && Atom::create(summary.c, Atom::OP_EQ, s.ret, entry.ret);
				}
				if (!(cond->check()))
					continue;

				warn = true;
				s.ops.diff(entry.ops, summary.tainted);
				if (summary.tainted.size() > tainted_size && (O_TEST || single_fn_mode)) {
					// Update @refcount_sig and in operations for reporting 
					for (auto &pair : entry.ops)
						pair.second.refcount_sig = pair.first;
					errs() << "\n----- " << F.getName()
					       << " (" << getInitialName(&F) << "@" << getLocation(&F) << ")"
					       << ": two cases disagrees with each other\n";
					errs() << "-- Case 1 (existing summary):\n";
					s.print(errs(), PATHTYPE, &i, &entry, "--     ", "    ");
					errs() << "-- Case 2 (new summary):\n";
					entry.print(errs(), PATHTYPE, &i, &s, "--     ", "    ");

					if (!PLOT_CFG.empty()) {
						ipp_id ++;
						std::string filename;
						raw_string_ostream os(filename);
						os << PLOT_CFG << "/" << F.getName() << "-" << ipp_id << ".dot";
						os.flush();
						i.plot_path_pair(filename, s.path, entry.path);
					}
				}
				tainted_size = summary.tainted.size();
				break;
			}

			if (merged)
				continue;

			if (entry.pc->is_true() && entry.ops.empty() && entry.ret == "[0]")
				continue;

			for (auto &pair : entry.ops) {
				const RefcountSig &sig = pair.first;
				Operation &op = pair.second;
				op.refcount_sig = sig;
				op.host = getFunctionName(&F);
			}

			if (!warn)
				summary.summaries.emplace_back(entry);
			else
				summary.dropped_summaries.emplace_back(entry);
		}
	} */

	void run_on_function(CallGraphNode *N, Function &F) {
		int nr_branches = 0;
		//Summary *summary = NULL;
		StringRef fn = getFunctionName(&F);

		int paths = 0, subcases = 0;

		std::cout << "Hello" << std::endl;

		/* if (F.getName().equals(SINGLE_FN)) {
			single_fn_mode = true;
			F.dump();
		} */

		// llvm::outs() raises an IO error. So we use printf instead.
		/* progress ++;
		if (O_PROGRESS) {
			progress_buf.clear();
			progress_os << progress << "/" << total << "    " << fn;
			progress_os.flush();
			int l = printf("%5d/%5d    %s", progress, total, fn.data());
			printf("%*s", 71 - l, "");
			fflush(stdout);
		} */

		//sys::TimeValue start_time = sys::TimeValue::now();
		//bool pure = true;

		/* if (!single_fn_mode && deserialize_summary(F)) {
			try {
				summary = &summaryBase.at(&F);
			} catch (std::out_of_range &e) {}
			goto end;
		}
		if (has_predefined_summary(F)) {
			try {
				summary = &summaryBase.at(&F);
			} catch (std::out_of_range &e) {}
			goto end;
		}
		if (F.empty())
			goto end;
		if (fn.startswith("llvm.") || fn.startswith(" __llvm"))
			goto end;
		if (gexist(blacklist, F.getName()))
			goto end;

		for (auto &record : *N) {
			CallGraphNode *calledN = record.second;
			if (!calledN)
				continue;
			Function *calledF = calledN->getFunction();
			if (!isPure(*calledF)) {
				pure = false;
				break;
			}
		} */

		/* Functions of pointer type may return one of its argument
		 * (e.g. get_device). What the function may return is useful in
		 * the analysis.
		 *
		 * UPDATE: the main usage of return values in pointer is
		 * summarizing attribute getters of structures. Thus we do not
		 * consider functions returning pointers to integers (characters
		 * are 1-byte integers). Functions with too many paths are also
		 * excluded.
		 */

		/* if (pure) {
			if (!gexist(sensilist, fn))
				goto end;
			for_each_inst(F, inst) {
				if (auto bi = dyn_cast<BranchInst>(inst)) {
					if (bi->isConditional())
						nr_branches ++;
				} else if (auto si = dyn_cast<SwitchInst>(inst)) {
					nr_branches ++;
				}
			}
			if (nr_branches > 3)
				goto end;
		} */

		/* ipp_id = 0;
		summary = &summaryBase[&F];
		summary->c.F = &F;
		summary->F = &F;
		summary->name = getFunctionName(&F);
		for (auto i = path_begin(F, summary->c), e = path_end(F, summary->c); i != e && paths < MaxPath; ++i, ++paths) {
			PathSummary path_summary;
			if (single_fn_mode) {
				errs() << "========== Path " << paths << " ==========\n";
			}

			if (run_on_path(F, summary->c, path_summary, i, paths)) {
				RangeToConstant r2c(summary->c);
				RemoveLocals rl(summary->c);
				for (auto &entry : path_summary) {
					summary->c.switch_pathid(entry.id);
					Formula mid;

					mid = entry.pc->deep_simplify();
					mid = r2c.visit(mid);
					StringRef new_ret = r2c.get_return();
					if (!new_ret.empty())
						entry.ret = new_ret.str();
					mid = mid->simplify();

					entry.exact_pc = mid;
					entry.pc = rl.visit(mid);
					if (entry.pc.get() == NULL)
						entry.pc = True::get(summary->c);
				}
				subcases += path_summary.size();
				merge_path(*summary, path_summary, F, i);
			}
		} */

		/* We cannot eliminate no-op cases in the summary as it will
		 * prevent us from distinguishing no-op cases and infeasible
		 * cases (e.g. requiring the return value of a function to be 2
		 * when it can only be 0 or 1).
		 *
		 * TODO: Look for a better summary form in which no-op summaries
		 * are not necessary.
		 */

		/* Simplify the PCs in the summary one last time, as it can get
		 * complicated by merging multiple path summaries.
		 */
		/* for (auto &s : *summary)
			s.pc = s.pc->deep_simplify(); */

		/* If the pcs do not cover all cases, add another one to make it
		 * complete
		 */
		/*if (summary->summaries.size() <= 3) {
			Formula remains = True::get(summary->c);
			for (auto &s : *summary)
				remains = remains && (!s.pc);
			remains = remains->deep_simplify();
			if (remains->check()) {
				summary->summaries.emplace_back();
				auto &s = summary->summaries.back();
				s.pc = remains;
				s.ret = "[0]";
			}
		}*/

		/* Empty summaries means the function has neither refcount
		 * operations nor usable return values. Throw the whole summary
		 * away here.
		 */
		/*if (summary->summaries.empty()) {
			summaryBase.erase(&F);
			summary = NULL;
		}

	end:
		serialize_summary(F);

		sys::TimeValue elapsed_time = sys::TimeValue::now();
		elapsed_time -= start_time;
		if (O_PROGRESS) {
			printf("(%2ld.%03us)  %4d,%5d\n", elapsed_time.seconds(), elapsed_time.milliseconds(), paths, subcases);
		}

		if (summary && (O_TEST || single_fn_mode)) {
			static bool newline = false;
			if (newline) {
				errs() << "\n";
			}
			if (summary->print(errs()))
				newline = true;
		}

		if (single_fn_mode)
			exit(0);*/
	}

public:
	static char ID;

	RSC() : CallGraphSCCPass(ID),
		progress_os(progress_buf),
		single_fn_mode(false),
		ipp_id(0)
		{}

	virtual bool doInitialization(CallGraph &CG) {
		Module &M = CG.getModule();

		//initializeDebugInfo(M);

		progress = 0;
		total = M.size();

		//cache_init();

		/*std::string line;
		std::ifstream fin(BLACKLIST);
		if (fin.is_open()) {
			while (std::getline(fin, line))
				blacklist.push_back(line);
			fin.close();
		}

		fin.open(SENSILIST);
		if (fin.is_open()) {
			while (std::getline(fin, line))
				sensilist.push_back(line);
			fin.close();
		}*/

		return false;
	}

	virtual bool runOnSCC(CallGraphSCC &SCC) {
		for (auto node : SCC) {
			Function *F = node->getFunction();
			if (!F)
				continue;
			run_on_function(node, *F);
		}

		return false;
	}

	virtual bool doFinalization(CallGraph &CG) {
		//cache_finalize();
		return false;
	}

	virtual void print(raw_ostream &O, const Module *M) const {}
};

char RSC::ID = 0;

static RegisterPass<RSC> X("rsc", "RT-kernel Sleep-in-atomic Context",
			   false /* Only looks at CFG */,
			   false /* Analysis Pass */);
