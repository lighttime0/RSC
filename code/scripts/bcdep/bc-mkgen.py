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

parser = argparse.ArgumentParser()
parser.add_argument('-o', '--output', type=str, default='Makefile')
parser.add_argument('-d', '--output_dir', type=str, default='linux')
parser.add_argument('database', type=str)
parser_args = parser.parse_args()

output = parser_args.output
outdir = parser_args.output_dir
db = parser_args.database

conn = sqlite3.connect(db)

bcid_to_bc = {}
bcid_to_scc = {}
bcs_in_scc = collections.defaultdict(lambda: [])
bc_is_eff = {}

scc_dep = collections.defaultdict(lambda: set())
scc_deped = set()

cur = conn.cursor()
cur.execute('SELECT * FROM bc')
for row in cur.fetchall():
    bcid, f, scc, eff = row[0], row[1], row[2], row[3]
    bcid_to_bc[bcid] = f
    bcid_to_scc[bcid] = scc
    bcs_in_scc[scc].append(f)
    bc_is_eff[f] = True if eff == 1 else False

cur.execute('SELECT * FROM dep')
for row in cur.fetchall():
    definer, user = row[0], row[1]
    scc_dep[bcid_to_scc[user]].add(bcid_to_scc[definer])
    scc_deped.add(bcid_to_scc[definer])



common_prefix = commonprefix(bcid_to_bc.values())
targets = {}
scc_files = {}
for scc,bcs in bcs_in_scc.items():
    if len(bcs) < 2:
        continue
    scc_prefix = commonprefix(bcs)[len(common_prefix):]
    target = os.path.join(outdir, 'scc', scc_prefix, 'scc%d.bc' % scc)
    ingredients = [bc for bc in bcs if bc_is_eff[bc]]
    if ingredients:
        targets[target] = ingredients
        scc_files[scc] = target

for scc in scc_dep.keys():
    if scc in scc_deped:
        continue
    bcs = bcs_in_scc[scc]
    mod_prefix = commonprefix(bcs)[len(common_prefix):]
    if mod_prefix.endswith('.bc'):
        target = os.path.join(outdir, 'modules', mod_prefix)
    else:
        target = os.path.join(outdir, 'modules', mod_prefix, 'scc%d.bc' % scc)
    ingredients = set()
    visited = collections.defaultdict(lambda: False)
    sccs = [scc]
    while len(sccs) > 0:
        scc = sccs.pop()
        if visited[scc]:
            continue
        if scc_files.has_key(scc):
            ingredients.add(scc_files[scc])
        else:
            for bc in bcs_in_scc[scc]:
                ingredients.add(bc)
        for dep in scc_dep[scc]:
            sccs.append(dep)
        visited[scc] = True
    ingredients = [bc for bc in ingredients if bc in scc_files.values() or bc_is_eff[bc]]
    if ingredients:
        targets[target] = ingredients

f = open(output, 'w')
print >> f, 'PREFIX ?=', common_prefix
print >> f, 'V ?= @'
print >> f, 'all:', ' '.join(targets.keys()).replace(common_prefix, '$(PREFIX)')
for target,ingredients in targets.items():
    print >> f, ''
    print >> f, '%s: %s' % (target.replace(common_prefix, '$(PREFIX)'), ' '.join(ingredients).replace(common_prefix, '$(PREFIX)'))
    print >> f, '\t@echo LINK $@'
    print >> f, '\t$(V)mkdir -p `dirname $@`'
    print >> f, '\t$(V)llvm-link -o $@ $+'
