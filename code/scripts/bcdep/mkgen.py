#!/usr/bin/env python

import sys, os
import argparse
import sqlite3
import collections

def commonprefix(m):
    "Given a list of pathnames, returns the longest common leading component"
    if not m: return ''
    a, b = min(m), max(m)
    lo, hi = 0, min(len(a), len(b))
    while lo < hi:
        mid = (lo+hi)//2 + 1
        if a[lo:mid] == b[lo:mid]:
            lo = mid
        else:
            hi = mid - 1
    return a[:hi]

def toposort2(data):
    """Dependencies are expressed as a dictionary whose keys are items
and whose values are a set of dependent items. Output is a list of
sets in topological order. The first set consists of items with no
dependences, each subsequent set consists of items that depend upon
items in the preceeding sets.

>>> print '\\n'.join(repr(sorted(x)) for x in toposort2({
...     2: set([11]),
...     9: set([11,8]),
...     10: set([11,3]),
...     11: set([7,5]),
...     8: set([7,3]),
...     }) )
[3, 5, 7]
[8, 11]
[2, 9, 10]

"""

    from functools import reduce

    # Ignore self dependencies.
    for k, v in data.items():
        v.discard(k)
    # Find all items that don't depend on anything.
    extra_items_in_deps = reduce(set.union, data.itervalues()) - set(data.iterkeys())
    # Add empty dependences where needed
    data.update({item:set() for item in extra_items_in_deps})
    while True:
        ordered = set(item for item, dep in data.iteritems() if not dep)
        if not ordered:
            break
        yield ordered
        data = {item: (dep - ordered)
                for item, dep in data.iteritems()
                    if item not in ordered}
    assert not data, "Cyclic dependencies exist among these items:\n%s" % '\n'.join(repr(x) for x in data.iteritems())

parser = argparse.ArgumentParser()
parser.add_argument('-o', '--output', type=str, default='Makefile')
parser.add_argument('-d', '--output-dir', type=str, default='linux')
parser.add_argument('-t', '--top-dir', type=str, default='')
parser.add_argument('database', type=str)
parser_args = parser.parse_args()

output = parser_args.output
outdir = parser_args.output_dir
topdir = parser_args.top_dir
db = parser_args.database

conn = sqlite3.connect(db)

bcid_to_bc = {}
bcid_to_scc = {}
bcs_in_scc = collections.defaultdict(lambda: [])

scc_dep_on = collections.defaultdict(lambda: set())
scc_dep_by = collections.defaultdict(lambda: set())

cur = conn.cursor()
cur.execute('SELECT * FROM bc')
for row in cur.fetchall():
    bcid, f, scc = row[0], row[1], row[2]
    bcid_to_bc[bcid] = f
    bcid_to_scc[bcid] = scc
    bcs_in_scc[scc].append(f)

cur.execute('SELECT * FROM dep')
for row in cur.fetchall():
    definer, user = row[0], row[1]
    scc_dep_on[bcid_to_scc[user]].add(bcid_to_scc[definer])
    scc_dep_by[bcid_to_scc[definer]].add(bcid_to_scc[user])


common_prefix = commonprefix(bcid_to_bc.values())
scc_to_bc = {}
scc_to_result = {}
to_be_linked= []

f = open(output, 'w')
fnf = open(output + '.fn', 'w')
sccf = open(output + '.scc', 'w')
listf = open('scc-list', 'w')
print >> f, 'PREFIX ?=', common_prefix
print >> f, 'V ?= @'
print >> f, 'TOPDIR ?=', topdir
print >> sccf, 'PREFIX ?=', common_prefix
print >> sccf, 'V ?= @'
print >> sccf, 'TOPDIR ?=', topdir
print >> fnf, 'PREFIX ?=', common_prefix
print >> fnf, 'V ?= @'
print >> fnf, 'TOPDIR ?=', topdir

for scc,bcs in bcs_in_scc.items():
    scc_prefix = commonprefix(bcs)[len(common_prefix):]
    bc = None
    if len(bcs) >= 2:
        target = os.path.join(outdir, 'scc', scc_prefix, 'scc%d.bc' % scc).replace(common_prefix, '$(PREFIX)')
        print >> sccf, ''
        print >> sccf, '%s: %s' % (target, ' '.join(bcs).replace(common_prefix, '$(PREFIX)'))
        print >> sccf, '\t@echo LINK $@'
        print >> sccf, '\t$(V)mkdir -p `dirname $@`'
        print >> sccf, '\t$(V)llvm-link -o $@ $+'
        to_be_linked.append(target)
        bc = target
    else:
        bc = bcs[0]

    if scc_prefix.endswith('.bc'):
        target = os.path.join(outdir, 'results', scc_prefix.replace('.bc', '.result')).replace(common_prefix, '$(PREFIX)')
    else:
        target = os.path.join(outdir, 'results', scc_prefix, 'scc%d.result' % scc).replace(common_prefix, '$(PREFIX)')

    scc_to_bc[scc] = bc
    scc_to_result[scc] = target

for scc,bcs in bcs_in_scc.items():
    bc = scc_to_bc[scc]
    target = scc_to_result[scc]

    target_deps = target.replace('.result', '.deps')
    time_log = target.replace('.result', '.time')
    out_log = target.replace('.result', '.log')
    sensilist_phase1 = target.replace('.result', '.sensi1')
    sensilist_phase2 = target.replace('.result', '.sensi2')

    deps = [scc_to_result[dep] for dep in scc_dep_on[scc] if dep != scc]
    if deps:
        print >> f, ''
        print >> f, '%s: %s' % (target_deps, ' '.join(deps))
        print >> f, '\t@echo DEPS $@'
        print >> f, '\t$(V)mkdir -p `dirname $@`'
        print >> f, '\t$(V)$(TOPDIR)/cache-merge -o-cache %s $+' % target_deps
    else:
        target_deps = ''

    print >> f, ''
    print >> f, '%s: %s %s' % (target, bc, target_deps)
    print >> f, '\t@echo RID  $@'
    print >> f, '\t$(V)mkdir -p `dirname $@`'
    if target_deps:
        print >> f, '\t$(V)opt -analyze -quiet -load $(TOPDIR)/rid.so -rid %s -predefined dpm,ffs -sensilist sensi-list -o-progress -o-test -i-cache %s -o-cache %s -path-type bbpath > %s 2> %s' % (bc, target_deps, target, time_log, out_log)
    else:
        print >> f, '\t$(V)opt -analyze -quiet -load $(TOPDIR)/rid.so -rid %s -predefined dpm,ffs -sensilist sensi-list -o-progress -o-test -o-cache %s -path-type bbpath > %s 2> %s' % (bc, target, time_log, out_log)

    deps = [scc_to_result[dep].replace('.result', '.sensi1') for dep in scc_dep_on[scc] if dep != scc]
    print >> fnf, ''
    print >> fnf, '%s: %s %s' % (sensilist_phase1, bc, ' '.join(deps))
    print >> fnf, '\t@echo FN1   $@'
    print >> fnf, '\t$(V)mkdir -p `dirname $@`'
    if deps:
        print >> fnf, '\t$(V)cat %s | sort | uniq > $@' % ' '.join(deps)
    else:
        print >> fnf, '\t$(V)touch $@'
    print >> fnf, '\t$(V)opt -analyze -quiet -load $(TOPDIR)/rid.so -sensiset1 %s -sensilist $@' % bc

    deps = [scc_to_result[dep].replace('.result', '.sensi2') for dep in scc_dep_by[scc] if dep != scc]
    deps.append(sensilist_phase1)
    print >> fnf, ''
    print >> fnf, '%s: %s %s' % (sensilist_phase2, bc, ' '.join(deps))
    print >> fnf, '\t@echo FN2   $@'
    print >> fnf, '\t$(V)mkdir -p `dirname $@`'
    print >> fnf, '\t$(V)cat %s | sort | uniq > $@' % ' '.join(deps)
    print >> fnf, '\t$(V)opt -analyze -quiet -load $(TOPDIR)/rid.so -sensiset2 %s -sensilist $@' % bc

    scc_to_result[scc] = target

print >> f, ''
print >> f, 'all: %s' % ' '.join(scc_to_result.values())

print >> sccf, 'all: %s' % ' '.join(to_be_linked)

sources = []
for scc,bcs in bcs_in_scc.items():
    if not scc_dep_on[scc]:
        sources.append(scc_to_result[scc].replace('.result', '.sensi2'))
print >> fnf, ''
print >> fnf, 'all:', ' '.join(sources)
print >> fnf, '\t$(V)cat %s | sort | uniq > sensi-list' % ' '.join(sources)

for sccs in toposort2(scc_dep_on):
    for scc in sccs:
        print >> listf, scc_to_bc[scc]
