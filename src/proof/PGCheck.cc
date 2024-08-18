/*
 *  Copyright (c) 2013, Simone Fulvio Rollini <simone.rollini@gmail.com>
 *
 *  SPDX-License-Identifier: MIT
 *
 */

#include "PG.h"

#include <common/InternalException.h>
#include <common/VerificationUtils.h>

#include <deque>

namespace opensmt {
void ProofGraph::verifyLeavesInconsistency() {
    if (verbose() > 0) { std::cerr << "# Verifying unsatisfiability of the set of proof leaves" << std::endl; }

    std::vector<clauseid_t> proofleaves;
    std::vector<clauseid_t> q;
    std::vector<unsigned> visited_count(getGraphSize(), 0u);
    q.push_back(getRoot()->getId());
    do {
        clauseid_t id = q.back();
        ProofNode * node = getNode(id);
        assert(node);
        visited_count[id]++;
        q.pop_back();

        // All resolvents have been visited
        if (id == getRoot()->getId() or visited_count[id] == node->getNumResolvents()) {
            if (not node->isLeaf()) {
                clauseid_t id1 = node->getAnt1()->getId();
                clauseid_t id2 = node->getAnt2()->getId();
                // Enqueue antecedents
                assert(visited_count[id1] < node->getAnt1()->getNumResolvents());
                assert(visited_count[id2] < node->getAnt2()->getNumResolvents());
                q.push_back(id1);
                q.push_back(id2);
            } else {
                proofleaves.push_back(id);
            }
        }
    } while (!q.empty());

    vec<PTRef> clauses;
    for (clauseid_t leaf_id : leaves_ids) {
        auto const & clause = getNode(leaf_id)->getClause();
        vec<PTRef> lits;
        lits.capacity(clause.size());
        for (Lit l : clause) {
            lits.push(termMapper.litToPTRef(l));
        }
        clauses.push(logic_.mkOr(std::move(lits)));
    }
    bool unsat = VerificationUtils(logic_).impliesInternal(logic_.mkAnd(std::move(clauses)), logic_.getTerm_false());
    if (not unsat) { throw std::logic_error("The set of proof leaves is satisfiable!"); }
}

bool ProofNode::checkPolarityAnt() {
    assert(getAnt1());
    assert(getAnt2());
    std::vector<Lit> & cla = getAnt1()->getClause();
    for (size_t i = 0; i < cla.size(); i++)
        if (var(cla[i]) == getPivot()) { return not sign(cla[i]); }
    throw InternalException("Pivot not found in node's clause");
}

void ProofGraph::checkClauseSorting(clauseid_t nid) {
    ProofNode * n = getNode(nid);
    assert(n);
    assert(n->getId() == nid);

    if (n->getClauseSize() == 0) return;

    for (size_t i = 0; i < n->getClauseSize() - 1; i++) {
        if (var(n->getClause()[i]) > var(n->getClause()[i + 1])) {
            std::cerr << "Bad clause sorting for clause " << n->getId() << " of type " << n->getType() << '\n';
            printClause(n);
            throw InternalException();
        }
        if (var(n->getClause()[i]) == var(n->getClause()[i + 1]) &&
            sign(n->getClause()[i]) == sign(n->getClause()[i + 1])) {
            std::cerr << "Repetition of var " << var(n->getClause()[i]) << " in clause " << n->getId() << " of type "
                      << n->getType() << '\n';
            printClause(n);
            throw InternalException();
        }
        if (var(n->getClause()[i]) == var(n->getClause()[i + 1]) &&
            sign(n->getClause()[i]) != sign(n->getClause()[i + 1])) {
            std::cerr << "Inconsistency on var " << var(n->getClause()[i]) << " in clause " << n->getId() << " of type "
                      << n->getType() << '\n';
            printClause(n);
            throw InternalException();
        }
    }
}

void ProofGraph::checkClause(clauseid_t nid) {
    ProofNode * n = getNode(nid);
    assert(n);
    assert(n->getId() == nid);

    // Check if empty clause
    if (isRoot(n)) {
        if (n->getClauseSize() != 0) {
            std::cerr << n->getId() << " is the sink but not an empty clause" << '\n';
            printClause(n);
            throw InternalException();
        }
    }
    if (n->getClauseSize() == 0) {
        if (n->getType() == clause_type::CLA_ORIG) {
            std::cerr << n->getId() << " is an empty original clause" << '\n';
            throw InternalException();
        }
    } else
        checkClauseSorting(n->getId());

    if (!n->isLeaf()) {
        assert(n->getId() != n->getAnt1()->getId() && n->getId() != n->getAnt2()->getId());
        assert(getNode(n->getAnt1()->getId()));
        assert(getNode(n->getAnt2()->getId()));

        if (n->getClauseSize() != 0) {
            std::vector<Lit> v;
            mergeClauses(n->getAnt1()->getClause(), n->getAnt2()->getClause(), v, n->getPivot());
            if (v.size() != n->getClauseSize()) {
                std::cerr << "Clause : ";
                printClause(n);
                std::cerr << " does not correctly derive from antecedents " << '\n';
                printClause(getNode(n->getAnt1()->getId()));
                printClause(getNode(n->getAnt2()->getId()));
                throw InternalException();
            }
            for (size_t i = 0; i < n->getClauseSize(); i++)
                if (n->getClause()[i] != v[i]) {
                    std::cerr << "Clause : ";
                    printClause(n);
                    std::cerr << " does not correctly derive from antecedents " << '\n';
                    printClause(getNode(n->getAnt1()->getId()));
                    printClause(getNode(n->getAnt2()->getId()));
                    throw InternalException();
                }
            // Checks whether clause is tautological
            std::vector<Lit> & cl = n->getClause();
            for (unsigned u = 0; u < cl.size() - 1; u++)
                if (var(cl[u]) == var(cl[u + 1])) {
                    std::cerr << "Clause : ";
                    printClause(n);
                    std::cerr << " is tautological " << '\n';
                }
            // Checks whether both antecedents have the pivot
            short f1 = n->getAnt1()->hasOccurrenceBin(n->getPivot());
            short f2 = n->getAnt2()->hasOccurrenceBin(n->getPivot());
            assert(f1 != -1);
            assert(f2 != -1);
            assert(f1 != f2);
            (void)f1;
            (void)f2;
        }
    }
    // Check that every resolvent has this node as its antecedent
    std::set<clauseid_t> & resolvents = n->getResolvents();
    for (clauseid_t id : resolvents) {
        assert(id < getGraphSize());
        ProofNode * res = getNode(id);
        if (res == NULL) {
            std::cerr << "Node " << n->getId() << " has resolvent " << id << " null" << '\n';
            throw InternalException();
        } else
            assert(res->getAnt1() == n || res->getAnt2() == n);
    }
}

void ProofGraph::checkProof(bool check_clauses) {
    if (verbose()) { std::cerr << "# Checking proof" << '\n'; }

    // Visit top down
    std::deque<clauseid_t> q;
    std::vector<unsigned> visit_level(getGraphSize());
    q.assign(leaves_ids.begin(), leaves_ids.end());
    do {
        clauseid_t id = q.front();
        ProofNode * n = getNode(id);
        assert(n);
        q.pop_front();
        if (!isSetVisited2(id)) {
            setVisited2(id);
            // Leaves are seen only once
            if (n->isLeaf()) {
                visit_level[id] = 1;
                for (clauseid_t resolvent_id : n->getResolvents()) {
                    q.push_back(resolvent_id);
                }
            }
        } else {
            assert(!n->isLeaf());
            assert(visit_level[id] == 0);
            // Inner should be seen twice
            for (clauseid_t resolvent_id : n->getResolvents()) {
                assert(visit_level[resolvent_id] == 0);
                q.push_back(resolvent_id);
            }

            clauseid_t id1 = n->getAnt1()->getId();
            clauseid_t id2 = n->getAnt2()->getId();
            assert(visit_level[id1] > 0);
            assert(visit_level[id2] > 0);
            visit_level[id] = visit_level[id1] > visit_level[id2] ? visit_level[id1] + 1 : visit_level[id2] + 1;
        }
    } while (!q.empty());

    // Visit bottom up
    q.push_back(getRoot()->getId());
    do {
        clauseid_t id = q.back();
        ProofNode * node = getNode(id);
        assert(node);
        q.pop_back();

        if (!node->isLeaf()) {
            clauseid_t id1 = node->getAnt1()->getId();
            clauseid_t id2 = node->getAnt2()->getId();
            assert(node->getAnt1() != node->getAnt2());
            // Enqueue antecedents the first time the node is seen
            if (!isSetVisited1(id)) {
                q.push_back(id1);
                q.push_back(id2);
            }
            if (check_clauses) checkClause(id);
        }
        setVisited1(node->getId());
    } while (!q.empty());

    // Ensure that the same nodes have been visited top-down and bottom-up
    for (unsigned u = 0; u < getGraphSize(); u++) {
        if (isSetVisited1(u) && !isSetVisited2(u)) {
            std::cerr << "Node " << u << " is unreachable going top-down" << '\n';
            throw InternalException();
        }
        if (!isSetVisited1(u) && isSetVisited2(u)) {
            std::cerr << "Node " << u << " is unreachable going bottom-up" << '\n';
            throw InternalException();
        }
    }

    // Ensure that there are no useless leaves
    for (clauseid_t leave_id : leaves_ids) {
        if (not isSetVisited1(leave_id)) {
            std::cerr << "Detached leaf" << leave_id << '\n';
            throw InternalException();
        }
    }

    resetVisited1();
    resetVisited2();
}
} // namespace opensmt
