#!/usr/bin/env python

import sys, os
import argparse
import sqlite3
import collections
from subprocess import Popen, PIPE

from scc import SCC

SEEDS = {'kref_init', 'kref_get', 'kref_get_unless_zero',
         'kref_put', 'kref_put_spinlock_irqsave', 'kref_put_mutex',
         'pm_runtime_get', 'pm_runtime_get_sync', 'pm_runtime_get_noresume', 'pm_runtime_forbid',
         'pm_runtime_put', 'pm_runtime_put_noidle', 'pm_runtime_put_autosuspend',
         'pm_runtime_put_sync', 'pm_runtime_put_sync_suspend', 'pm_runtime_put_sync_autosuspend', 'pm_runtime_allow'}

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--database', type=str, default='dep.db')
parser.add_argument('bclist', type=str)
parser_args = parser.parse_args()

bclist = parser_args.bclist
db = parser_args.database

conn = sqlite3.connect(db)
tables = {
    'bc':
    [ 'id INTEGER PRIMARY KEY AUTOINCREMENT',
      'file TEXT NOT NULL',
      'scc INTEGER',
      'effective INTEGER'],
    'dep':
    [ 'definer INTEGER NOT NULL',
      'user INTEGER NOT NULL'],
}
for k,v in tables.items():
    conn.execute('CREATE TABLE IF NOT EXISTS %s(%s);' % (k, ','.join(v)))

bc_id = {}
defined_in = {}
used_in = collections.defaultdict(lambda: [])
weak_in = collections.defaultdict(lambda: [])

f = open(bclist, 'r')
next_id = 1
for line in f.readlines():
    bc = line.strip()
    effective = 0

    cmd = 'llvm-nm ' + bc
    p = Popen(cmd, shell=True, stdin=None, stdout=PIPE, stderr=None, close_fds=True)
    out = [line.strip() for line in p.stdout]
    p.communicate()
    if p.returncode != 0:
        print bc, ': reading symbol file failed'
        continue
    p.stdout.close()
    for l in out:
        state, symbol = l.split(' ')
        if symbol in SEEDS:
            effective = 1
        elif state == 'T':
            defined_in[symbol] = bc
        elif state == 'U':
            used_in[symbol].append(bc)
        elif state == 'W':
            weak_in[symbol].append(bc)

    conn.execute('INSERT INTO bc (file, scc, effective) VALUES ("%s", 0, %d)' % (bc, effective))
    bc_id[bc] = next_id
    next_id = next_id + 1

deps = set()
succs = collections.defaultdict(lambda :[])
for k,v in used_in.items():
    if not defined_in.has_key(k):
        continue
    definer = bc_id[defined_in[k]]
    for u in v:
        user = bc_id[u]
        deps.add((definer, user))
        succs[definer].append(user)

for dep in deps:
    conn.execute('INSERT INTO dep VALUES (%d, %d)' % dep)

s = SCC(int, lambda i: succs[i])
sccs = s.getsccs(range(0, next_id))

sccid = 1
for scc in sccs:
    for v in scc:
        conn.execute('UPDATE bc SET scc = %d WHERE id = %d' % (sccid, v))
    sccid = sccid + 1

conn.commit()
