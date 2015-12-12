/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    unit_subsumption_tactic.cpp

Abstract:

    Simplify goal using subsumption based on unit propagation.

Author:

    Nikolaj Bjorner (nbjorner) 2012-9-6

--*/

#include "unit_subsumption_tactic.h"
#include "smt_context.h"

struct unit_subsumption_tactic : public tactic {
    ast_manager&    m;
    params_ref      m_params;
    smt_params      m_fparams;
    smt::context    m_context;
    expr_ref_vector m_clauses;
    unsigned        m_clause_count;
    bit_vector      m_is_deleted;
    unsigned_vector m_deleted;
           
    unit_subsumption_tactic(
        ast_manager& m, 
        params_ref const& p):
        m(m), 
        m_params(p), 
        m_context(m, m_fparams, p),
        m_clauses(m) {
    }

    void cleanup() {}

    virtual void operator()(/* in */  goal_ref const & in, 
                            /* out */ goal_ref_buffer & result, 
                            /* out */ model_converter_ref & mc, 
                            /* out */ proof_converter_ref & pc,
                            /* out */ expr_dependency_ref & core) {        
        reduce_core(in, result);
    }

    virtual void updt_params(params_ref const& p) {
        m_params = p;
        // m_context.updt_params(p); does not exist.
    }
    
    virtual tactic* translate(ast_manager& m) {
        return alloc(unit_subsumption_tactic, m, m_params);
    }
    
    void checkpoint() {
        if (m.canceled()) {
            throw tactic_exception(TACTIC_CANCELED_MSG);
        }
    }

    void reduce_core(goal_ref const& g, goal_ref_buffer& result) {
        init(g);
        m_context.push();
        assert_clauses(g);
        m_context.push();   // internalize assertions. 
        prune_clauses();
        goal_ref r(g);
        insert_result(r);
        r->elim_true();
        result.push_back(r.get());        
        m_context.pop(2);
        TRACE("unit_subsumption_tactic", g->display(tout); r->display(tout););
    }

    void assert_clauses(goal_ref const& g) {
        for (unsigned i = 0; i < g->size(); ++i) {
            m_context.assert_expr(m.mk_iff(new_clause(), g->form(i)));
        }
    }

    void prune_clauses() {
        for (unsigned i = 0; i < m_clause_count; ++i) {
            prune_clause(i);
        }
    }

    void prune_clause(unsigned i) {
        m_context.push();
        for (unsigned j = 0; j < m_clause_count; ++j) {
            if (i == j) {
                m_context.assert_expr(m.mk_not(m_clauses[j].get()));
            }
            else if (!m_is_deleted.get(j)) {
                m_context.assert_expr(m_clauses[j].get());
            }
        }
        m_context.push(); // force propagation
        bool is_unsat = m_context.inconsistent();
        m_context.pop(2);
        if (is_unsat) {            
            TRACE("unit_subsumption_tactic", tout << "Removing clause " << i << "\n";);
            m_is_deleted.set(i, true);
            m_deleted.push_back(i);
        }
    }

    void insert_result(goal_ref& result) {        
        for (unsigned i = 0; i < m_deleted.size(); ++i) {
            result->update(m_deleted[i], m.mk_true()); // TBD proof?
        }
    }

    void init(goal_ref const& g) {
        m_clause_count = 0;
        m_is_deleted.reset();
        m_is_deleted.resize(g->size());
        m_deleted.reset();
    }

    expr* new_bool(unsigned& count, expr_ref_vector& v, char const* name) {
        SASSERT(count <= v.size());
        if (count == v.size()) {
            v.push_back(m.mk_fresh_const(name, m.mk_bool_sort()));
        }
        return v[count++].get();
    }

    expr* new_clause() {
        return new_bool(m_clause_count, m_clauses, "#clause");
    }

    
};

tactic * mk_unit_subsumption_tactic(ast_manager & m, params_ref const & p) {
    return alloc(unit_subsumption_tactic, m, p);
}

