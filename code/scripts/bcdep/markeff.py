#!/usr/bin/env python

import sys, os
import argparse
import sqlite3
import collections

parser = argparse.ArgumentParser()
parser.add_argument('database', type=str)
parser_args = parser.parse_args()

db = parser_args.database
conn = sqlite3.connect(db)

bcid_is_eff = {}
bcid_visited = collections.defaultdict(lambda: False)
bcid_dep = collections.defaultdict(lambda: set())

cur = conn.cursor()
cur.execute('SELECT id, effective FROM bc')
for row in cur.fetchall():
    bcid, eff = row[0], row[1]
    bcid_is_eff[bcid] = True if eff == 1 else False

cur.execute('SELECT * FROM dep')
for row in cur.fetchall():
    definer, user = row[0], row[1]
    bcid_dep[definer].add(user)

while True:
    changed = False
    for k,v in bcid_dep.items():
        if bcid_is_eff[k] and not bcid_visited[k]:
            bcid_visited[k] = True
            for i in v:
                if not bcid_is_eff[i]:
                    bcid_is_eff[i] = True
                    changed = True
    if not changed:
        break

for k,v in bcid_is_eff.items():
    if not v:
        continue
    conn.execute('UPDATE bc SET effective = 1 WHERE id = %d' % k)
conn.commit()
