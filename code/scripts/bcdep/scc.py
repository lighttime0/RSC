import sys

class SCC(object):
    def __init__(self, node2id, successors):
        self.node2id = node2id
        self.successors = successors

    def getsccs(self, roots):
        node2id, successors = self.node2id, self.successors
        get_dfsnum = iter(xrange(sys.maxint)).next
        id2dfsnum = {}
        id2lowest = {}
        stack = []
        id2stacki = {}
        sccs = []

        def dfs(v, vid):
            id2dfsnum[vid] = id2lowest[vid] = v_dfsnum = get_dfsnum()
            id2stacki[vid] = len(stack)
            stack.append((v, vid))
            for w in successors(v):
                wid = node2id(w)
                if wid not in id2dfsnum:   # first time we saw w
                    dfs(w, wid)
                    id2lowest[vid] = min(id2lowest[vid], id2lowest[wid])
                else:
                    w_dfsnum = id2dfsnum[wid]
                    if w_dfsnum < v_dfsnum and wid in id2stacki:
                        id2lowest[vid] = min(id2lowest[vid], w_dfsnum)

            if id2lowest[vid] == v_dfsnum:
                i = id2stacki[vid]
                scc = []
                for w, wid in stack[i:]:
                    del id2stacki[wid]
                    scc.append(w)
                del stack[i:]
                sccs.append(scc)

        for v in roots:
            vid = node2id(v)
            if vid not in id2dfsnum:
                dfs(v, vid)
        sccs.reverse()
        return sccs
