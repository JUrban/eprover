/*-----------------------------------------------------------------------

File  : ccl_subsumption.c

Author: Stephan Schulz

Contents
 
  Functions for subsumption of clauses.

  Copyright 1998, 1999 by the author.
  This code is released under the GNU General Public Licence.
  See the file COPYING in the main CLIB directory for details.
  Run "eprover -h" for contact information.

Changes

<1> Sun Jun  7 15:12:29 MET DST 1998
    New

-----------------------------------------------------------------------*/

#include "ccl_subsumption.h"


/*---------------------------------------------------------------------*/
/*                        Global Variables                             */
/*---------------------------------------------------------------------*/

bool StrongUnitForwardSubsumption = false;
long ClauseClauseSubsumptionCalls = 0;
long ClauseClauseSubsumptionCallsRec = 0;

/*---------------------------------------------------------------------*/
/*                      Forward Declarations                           */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                         Internal Functions                          */
/*---------------------------------------------------------------------*/



/*-----------------------------------------------------------------------
//
// Function: unit_clause_set_strongsubsumes_termpair()
//
//   Return a unit clause with sign positive from set if there is a
//   subset with sign positive that shows t1=t2 in one step. Return
//   NULL otherwise.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static 
ClausePos_p unit_clause_set_strongsubsumes_termpair(ClauseSet_p set, 
						    Term_p t1, Term_p t2,
						    bool positive)
{
   PStack_p stack = PStackAlloc();
   int      i;
   ClausePos_p res = NULL;
   
   PStackPushP(stack, t1);
   PStackPushP(stack, t2);
   
   while(!PStackEmpty(stack))
   {
      t2 = PStackPopP(stack);
      t1 = PStackPopP(stack);
      res = FindSignedTopSimplifyingUnit(set, t1, t2, positive);
      if(!res)
      {
	 if(t1->f_code != t2->f_code || !t1->arity)
	 {
	    break;
	 }
	  for(i=0; i<t1->arity; i++)
	  {
	     if(!TBTermEqual(t1->args[i], t2->args[i]))
	     {
		PStackPushP(stack, t1->args[i]);
		PStackPushP(stack, t2->args[i]);
	     }
	  }
      }
      
   }
   PStackFree(stack);
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: unit_clause_set_subsumes_clause()
//
//   Return a clause from set that subsumes clause.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static 
Clause_p unit_clause_set_subsumes_clause(ClauseSet_p set,
					 Clause_p clause)
{
   Eqn_p    handle = clause->literals;
   ClausePos_p res = NULL;
   
   while(handle)
   {
      if(EqnIsPositive(handle))
      {
	 res = StrongUnitForwardSubsumption?
	    unit_clause_set_strongsubsumes_termpair(set, handle->lterm,
						    handle->rterm,
						    true):
	    FindSimplifyingUnit(set, handle->lterm,
				handle->rterm,
				true);
      }
      else
      {
	 res = FindSignedTopSimplifyingUnit(set,
					    handle->lterm, 
					    handle->rterm,
					    false);
      }
      if(res)
      {
	 break;
      }
      handle = handle->next;
   }
   return res?res->clause:NULL;
}


/*-----------------------------------------------------------------------
//
// Function: eqn_topsubsumes_termpair()
//
//   Return true if eqn subsumes t1=t2 at top level.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static bool eqn_topsubsumes_termpair(Eqn_p eqn, Term_p t1, Term_p t2)
{
   Subst_p subst = SubstAlloc();
   bool    res = false;

   assert(eqn);
   assert(t1);
   assert(t2);

   if(SubstComputeMatch(eqn->lterm, t1, subst, TBTermEqual))
   {
      if(SubstComputeMatch(eqn->rterm,
			   t2, subst, TBTermEqual))
      {
	 res = true;
      }
   }
   else if(SubstComputeMatch(eqn->lterm, t2, subst, TBTermEqual))
   {
      if(SubstComputeMatch(eqn->rterm, t1, subst, TBTermEqual))
      {
	 res = true;
	 
      }
   }
   SubstDelete(subst);

   return res;
}


/*-----------------------------------------------------------------------
//
// Function: eqn_subsumes_termpair()
//
//   Return true if the equation subsumes t1=t2.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static bool eqn_subsumes_termpair(Eqn_p eqn, Term_p t1, Term_p t2)
{
   Term_p   tmp1, tmp2 = NULL;
   int      i;
   bool     res = false;
   
   assert(t1);
   assert(t2);
   assert(eqn);

   while(!(res = eqn_topsubsumes_termpair(eqn, t1, t2)))
   {
      if(t1->f_code != t2->f_code || !t1->arity)
      {
	 break;
      }
      assert(t1->arity == t2->arity);
      
      tmp1 = NULL;
      tmp2 = NULL;
      
      for(i=0; i<t1->arity; i++)
      {
	 if(!TBTermEqual(t1->args[i], t2->args[i]))
	 {
	    if(tmp1)
	    {
	       return false;
	    }
	    tmp1 = t1->args[i];
	    tmp2 = t2->args[i];	    
	 }
      }
      if(!tmp1)
      {
	 return true;
      }
      t1 = tmp1;
      t2 = tmp2;      
   }
   return res;
}

/*-----------------------------------------------------------------------
//
// Function: check_subsumption_condition()
//
//   Return true if subst cannot possibly be a variable renaming or if
//   no literal in the subsuming clause is used to subsume more than
//   one literal in the subsumed claues. This seems to be
//   insufficient, so I'm now using strict multiset-subsumption.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/
/*
bool check_subsumption_condition(Eqn_p sub_cand_list, Subst_p subst,
				 long *pick_list) 
{
   PStackPointer i;
   DerefType     deref;
   Eqn_p         eqn;
   long          lcount;
   bool          res = false;

   for(eqn = sub_cand_list, lcount=0; eqn; eqn = eqn->next, lcount++)
   {
      if(pick_list[lcount]>1)
      {
	 res = true;
	 break;
      }
   }
   if(!res)
   {
      return true;
   }
   for(i=0; i<PStackGetSP(subst); i++)
   {
      deref = DEREF_ONCE;
      if(!TermIsVar(TermDeref(PStackElementP(subst,i),
			      &deref)))
      {
	 return true;
      }
   }
   return false;
}*/


/*-----------------------------------------------------------------------
//
// Function: find_spec_literal()
//
//   Find a literal in list that is more special than lit. Return it
//   or NULL if none exists.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static Eqn_p find_spec_literal(Eqn_p lit, Eqn_p list)
{
   Subst_p subst = SubstAlloc();

   for(;list;list = list->next)
   {
      if(!PropsAreEquiv(lit, list, EPIsPositive|EPIsEquLiteral))
      {
	 continue;
      }
      if(EqnIsOriented(list) && !EqnIsOriented(lit))
      {
	 continue;
      }
      if(SubstComputeMatch(lit->lterm, list->lterm, subst,
			   TBTermEqual)&&
	 SubstComputeMatch(lit->rterm, list->rterm, subst,
			   TBTermEqual)) 
      {	 
	 break;
      }
      SubstBacktrack(subst);
      if(EqnIsOriented(lit))
      {
	 continue;
      }
      if(SubstComputeMatch(lit->lterm, list->rterm, subst,
			   TBTermEqual)&&
	 SubstComputeMatch(lit->rterm, list->lterm, subst,
			   TBTermEqual))
      {
	 break;
      }
      SubstBacktrack(subst);
   }
   SubstDelete(subst);
   return list;
}


/*-----------------------------------------------------------------------
//
// Function: check_subsumption_possibility()
//
//   Return true if each literal in subsumer is more general than a
//   literal in sub_candidate.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static bool check_subsumption_possibility(Clause_p subsumer, Clause_p
					  sub_candidate)
{
   bool    res = true;
   Eqn_p   sub_eqn;
   
   for(sub_eqn = subsumer->literals; sub_eqn; sub_eqn = sub_eqn->next)
   {
      if(!find_spec_literal(sub_eqn, sub_candidate->literals))
      {
	 res = false;
	 break;
      }
   }
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: eqn_list_rec_subsume()
//
//   Try to find a subset of sub_cand_list such that
//   subst(subsum_list) = subset. Return true if this is possible,
//   false otherwise.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static
bool eqn_list_rec_subsume(Eqn_p subsum_list, Eqn_p sub_cand_list,
			  Subst_p subst, long* pick_list)
{
   Eqn_p         eqn;
   PStackPointer state;
   int lcount;
   
   if(!subsum_list)
   {
      /* return check_subsumption_condition(sub_cand_list, subst,
       * pick_list);*/
      return true;
   }
   
   for(eqn = sub_cand_list, lcount=0; eqn; eqn = eqn->next, lcount++)
   {
      /* Some optimizations: Of course both equation need to have the
	 same sign. If the potentially more general equation
	 is oriented, then the potentially more specialized has to be
	 oriented as well. Also, if the potentially more specialized
	 equation is maximal, so has to be the more general one. */
      if(!PropsAreEquiv(eqn, subsum_list, EPIsPositive|EPIsEquLiteral))
      {
	 continue;
      }
      if(EqnIsOriented(subsum_list) && !EqnIsOriented(eqn))
      {
	 continue;
      }
      /* This assumption is no longer valid with selection (selection
	 works by making arbitrary negative literals
	 maximal). Moreover, for some strange reason it also slowed
	 down the ordinary case */
      /* if(EqnIsMaximal(eqn) && !EqnIsMaximal(subsum_list))
      {
	 continue;
      }  */
      /* We now use strict multiset-subsumption. I should probably
	 rewrite this code to be more efficient for that case...*/
      if(pick_list[lcount])
      {
	 continue;
      }
      pick_list[lcount]++;
      state = PStackGetSP(subst);
      
      if(SubstComputeMatch(subsum_list->lterm, eqn->lterm, subst,
			   TBTermEqual)&&
	 SubstComputeMatch(subsum_list->rterm, eqn->rterm,
			   subst, TBTermEqual))
      {	 
	 if(eqn_list_rec_subsume(subsum_list->next, sub_cand_list,
				    subst, pick_list))
	 {
	    return true;
	 }
      }
      SubstBacktrackToPos(subst, state);
      if(EqnIsOriented(subsum_list))
      {
	 state = PStackGetSP(subst);
	 pick_list[lcount]--;
	 continue;
      }
      if(SubstComputeMatch(subsum_list->lterm, eqn->rterm, subst,
			   TBTermEqual)&&
	 SubstComputeMatch(subsum_list->rterm, eqn->lterm,
			   subst, TBTermEqual))
      {
	 if(eqn_list_rec_subsume(subsum_list->next, sub_cand_list,
				 subst, pick_list))
	 {
	    return true;
	 }
      }
      SubstBacktrackToPos(subst, state);
      pick_list[lcount]--;
   }
   return false;      
}



/*-----------------------------------------------------------------------
//
// Function: clause_subsumes_clause()
//
//   Return true if subsumer subsumes sub_candidate. Assumes that
//   weights are precomputed.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static bool clause_subsumes_clause(Clause_p subsumer, Clause_p
				   sub_candidate)
{
   Subst_p subst;
   bool    res;
   long* pick_list;

   assert(ClauseLiteralNumber(subsumer) > 0);
   if(ClauseLiteralNumber(subsumer)==1)
   {
      return UnitClauseSubsumesClause(subsumer, sub_candidate);
   }
   assert(sub_candidate->weight == ClauseStandardWeight(sub_candidate));
   assert(subsumer->weight == ClauseStandardWeight(subsumer));

   ClauseClauseSubsumptionCalls++;

   if((subsumer->pos_lit_no > sub_candidate->pos_lit_no) ||
      (subsumer->neg_lit_no > sub_candidate->neg_lit_no))
   {
      return false;
   }
   if(subsumer->weight > sub_candidate->weight)
   {
      return false;
   }
   if(((sub_candidate->pos_lit_no >=3) ||
       (sub_candidate->neg_lit_no >=3))&&
      !check_subsumption_possibility(subsumer, sub_candidate))
   {
      return false;
   }
   subst = SubstAlloc();
   ClauseClauseSubsumptionCallsRec++;

   pick_list = IntArrayAlloc(ClauseLiteralNumber(sub_candidate));
   res = eqn_list_rec_subsume(subsumer->literals,
			      sub_candidate->literals, subst,
			      pick_list);
   IntArrayFree(pick_list, ClauseLiteralNumber(sub_candidate));

   SubstDelete(subst);

   return res;
}

/*-----------------------------------------------------------------------
//
// Function: clause_set_subsumes_clause()
//
//   Return true if the set subsumes sub_candidate. All clauses need
//   correct weights!
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static
bool clause_set_subsumes_clause(ClauseSet_p set, Clause_p sub_candidate)
{
   Clause_p handle;

   assert(ClauseLiteralNumber(sub_candidate)>1);
   assert(sub_candidate->weight ==
          ClauseStandardWeight(sub_candidate));
   
   for(handle = set->anchor->succ; handle != set->anchor;
       handle = handle->succ)
   {
      if(clause_subsumes_clause(handle, sub_candidate))
      {
         /* printf("\nTrue: ");
         ClausePrint(stdout, handle, true);
         printf("\n subsumes: ");
         ClausePrint(stdout, sub_candidate, true);
         printf("\n"); */
         DocClauseQuote(GlobalOut, OutputLevel, 6, sub_candidate,
                        "subsumed", handle);
         ClauseSetProp(handle, ClauseQueryProp(sub_candidate,CPIsSOS));
         return true;
      }
   }
   return false;
}


/*-----------------------------------------------------------------------
//
// Function: clause_tree_find_subsuming_clause()
//
//   Given a PTree of clauses and a clause, return true if one of the
//   clauses subsume the candidate.
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

static
bool clause_tree_find_subsuming_clause(PTree_p tree, Clause_p sub_candidate)
{
   Clause_p clause;
   
   if(!tree)
   {
      return false;
   }
   clause = tree->key;
   if(clause_subsumes_clause(clause,sub_candidate))
   {
      DocClauseQuote(GlobalOut, OutputLevel, 6, sub_candidate,
		     "subsumed", clause);
      ClauseSetProp(clause, ClauseQueryProp(sub_candidate,CPIsSOS));
      return true;
   }
   return clause_tree_find_subsuming_clause(tree->lson, sub_candidate)
      ||clause_tree_find_subsuming_clause(tree->rson, sub_candidate);
}

/*-----------------------------------------------------------------------
//
// Function: clause_set_subsumes_clause_indexed()
//
//   Return true if the set subsumes sub_candidate. All clauses need
//   correct weights!
//
// Global Variables: 
//
// Side Effects    : 
//
/----------------------------------------------------------------------*/

static
bool clause_set_subsumes_clause_indexed(FVIndex_p index, FreqVector_p vec, long feature)
{
   if(FVIndexFinalNode(index))
   {
      return clause_tree_find_subsuming_clause(index->u1.clauses, vec->clause);
   }
   else
   {
      long i;
      FVIndex_p next;
      
      for(i=0; i<=vec->freq_vector[feature]; i++)
      {
	 next = FVIndexGetNextNonEmptyNode(index, i);
	 if(next && 
	    clause_set_subsumes_clause_indexed(next, vec, feature+1))
	 {
	    return true;
	 }
      }
      return false;
   }
}

/*-----------------------------------------------------------------------
//
// Function: clause_tree_find_subsumed_clause()
//
//   Given a PTree of clauses and a clause, push all subsumed clauses
//   onto res.
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

static
void clause_tree_find_subsumed_clauses(PTree_p tree, Clause_p subsumer, 
				       PStack_p res)
{
   Clause_p clause;
   
   if(!tree)
   {
      return;
   }
   clause = tree->key;
   if(clause_subsumes_clause(subsumer, clause))
   {
      DocClauseQuote(GlobalOut, OutputLevel, 6, clause,
		     "subsumed", subsumer);
      ClauseSetProp(subsumer, ClauseQueryProp(clause,CPIsSOS));
      PStackPushP(res, clause);
   }
   clause_tree_find_subsumed_clauses(tree->lson, subsumer, res);
   clause_tree_find_subsumed_clauses(tree->rson, subsumer, res);
}


/*-----------------------------------------------------------------------
//
// Function: clauseset_find_subsumed_clauses();
//
//   Find all clauses subsumed by subsumer and push them onto
//   stack. Also write PCL statements to that effect (if required by
//   output level).
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

static
void clauseset_find_subsumed_clauses(ClauseSet_p set, 
				     Clause_p subsumer, 
				     PStack_p res)
{
   Clause_p handle;

   for(handle = set->anchor->succ; 
       handle!= set->anchor; 
       handle = handle->succ)
   {
      if(clause_subsumes_clause(subsumer, handle))
      {
	 DocClauseQuote(GlobalOut, OutputLevel, 6, handle,
			"subsumed", subsumer); 
	 PStackPushP(res, handle);
      }
   }
}


/*-----------------------------------------------------------------------
//
// Function: clauseset_find_subsumed_clauses_indexed()
//
//   Find all clauses subsumed by vec->clause in index and push them
//   onto res.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

static 
void clauseset_find_subsumed_clauses_indexed(FVIndex_p index, 
					     FreqVector_p vec, 
					     long feature, 
					     PStack_p res)
{
   if(FVIndexFinalNode(index))
   {
      clause_tree_find_subsumed_clauses(index->u1.clauses, vec->clause, res);
   }
   else
   {
      long i, limit;
      FVIndex_p next;
      
      limit = MAX(index->array_size, index->type_or_key+1); /* Hack!*/
      
      for(i=vec->freq_vector[feature]; i<limit; i++)
      {
	 next = FVIndexGetNextNonEmptyNode(index, i);
	 if(next)
	 {
	    clauseset_find_subsumed_clauses_indexed(next, vec, 
						    feature+1, res);
	 }
      }
   }   
}

/*---------------------------------------------------------------------*/
/*                         Exported Functions                          */
/*---------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
//
// Function: LiteralSubsumesClause()
//
//   Return true if literal subsumes one of the literals in clause
//   (otherwise return false).
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool LiteralSubsumesClause(Eqn_p literal, Clause_p clause)
{
   Eqn_p handle;

   handle = clause->literals;
   while(handle)
   {
      if(EqnIsPositive(literal))
      {
	 if(EqnIsPositive(handle) &&
	    eqn_subsumes_termpair(literal, handle->lterm,
				  handle->rterm))
	 {
	    return true;
	 }
      }
      else
      {
	 if(EqnIsNegative(handle) &&
	    eqn_topsubsumes_termpair(literal, handle->lterm,
				  handle->rterm))
	 {
	    return false;
	 }
      }	 
      handle = handle->next;
   }
   return false;
}


/*-----------------------------------------------------------------------
//
// Function: UnitClauseSubsumesClause()
//
//   Return true if unit subsumes clause.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool UnitClauseSubsumesClause(Clause_p unit, Clause_p clause)
{
   bool res;

   assert(ClauseLiteralNumber(unit) == 1);
   
   res = LiteralSubsumesClause(unit->literals, clause);
   if(res)
   {
      DocClauseQuote(GlobalOut, OutputLevel, 6, clause, "subsumed",
		     unit);
      ClauseSetProp(unit, ClauseQueryProp(clause,CPIsSOS));
   }
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: UnitClauseSetSubsumesClause()
//
//   If a clause in set subsumes clause, return a pointer to
//   it. Otherwise return NULL.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

Clause_p UnitClauseSetSubsumesClause(ClauseSet_p set, Clause_p
				     clause)
{
   Clause_p res;
   
   res = unit_clause_set_subsumes_clause(set, clause);   

   if(res)
   {
      DocClauseQuote(GlobalOut, OutputLevel, 6, clause, "subsumed",
		     res);
      ClauseSetProp(res, ClauseQueryProp(clause,CPIsSOS));      
   }

   return res;
}


/*-----------------------------------------------------------------------
//
// Function: ClauseSetFindUnitSubsumedClause()
//
//   Return a pointer to the first clause in the list at or after
//   set_position that is subsumed by the unit-clause subsumer. Return
//   NULL, if no such clause exists.
//
// Global Variables: 
//
// Side Effects    : 
//
/----------------------------------------------------------------------*/

Clause_p ClauseSetFindUnitSubsumedClause(ClauseSet_p set, Clause_p
					 set_position, Clause_p
					 subsumer)
{
   assert(ClauseLiteralNumber(subsumer) == 1);

   while(set_position != set->anchor)
   {
      if(UnitClauseSubsumesClause(subsumer, set_position))
      {
	 return set_position;
      }
      set_position = set_position->succ;
   }
   return NULL;
}
   

/*-----------------------------------------------------------------------
//
// Function: ClausePositiveSimplifyReflect()
//
//   Remove all negative literals subsumed by the positive unit
//   clauses in set from clause. Return true if clause is empty, false
//   otherwise. Set has to be indexed and should contain only positive
//   units!
//
// Global Variables: -
//
// Side Effects    : Changes clause
//
/----------------------------------------------------------------------*/

bool ClausePositiveSimplifyReflect(ClauseSet_p set, Clause_p clause)
{
   Eqn_p   *handle = &(clause->literals);
   ClausePos_p res = NULL;
   
   while(*handle)
   {
      res = NULL;
      if(!EqnIsPositive(*handle))
      {
	 res = StrongUnitForwardSubsumption?
	    unit_clause_set_strongsubsumes_termpair(set,
						    (*handle)->lterm,
						    (*handle)->rterm,
						    true):
	    FindSimplifyingUnit(set, 
				(*handle)->lterm,
				(*handle)->rterm,
				true);
      }
      if(res)
      {
	 ClauseRemoveLiteral(clause, handle);
	 if(ClauseQueryProp(res->clause, CPIsSOS))
	 {
	    ClauseSetProp(clause, CPIsSOS);
	 }
	 DocClauseModificationDefault(clause, inf_simplify_reflect,
				      res->clause);
      }
      else
      {
	 handle = &((*handle)->next);
      }
   }
   return (clause->literals ==  NULL);
}


/*-----------------------------------------------------------------------
//
// Function: ClauseNegativeSimplifyReflect()
//
//   Remove all positive literals subsumed by negative unit clauses
//   in set from clause. Return true if clause is empty, false
//   otherwise. Set has to be indexed and contain negative units
//   only.
//
// Global Variables: -
//
// Side Effects    : Changes clause
//
/----------------------------------------------------------------------*/

bool ClauseNegativeSimplifyReflect(ClauseSet_p set, Clause_p clause)
{
   Eqn_p   *handle = &(clause->literals);
   ClausePos_p res = NULL;
   
   while(*handle)
   {
      res = NULL;
      if(EqnIsPositive(*handle))
      {
	 res = FindSignedTopSimplifyingUnit(set,
					    (*handle)->lterm,
					    (*handle)->rterm, 
					    false);
      }
      if(res)
      {
	 ClauseRemoveLiteral(clause, handle);
	 if(ClauseQueryProp(res->clause, CPIsSOS))
	 {
	    ClauseSetProp(clause, CPIsSOS);
	 }
	 DocClauseModificationDefault(clause, inf_simplify_reflect,
				      res->clause);
      }
      else
      {
	 handle = &((*handle)->next);
      }
   }
   return (clause->literals ==  NULL);
}

/*-----------------------------------------------------------------------
//
// Function: ClauseSubsumesClause()
//
//   Return true if subsumer subsumes sub_candidate. Requires that
//   both clauses have correct weight information.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool ClauseSubsumesClause(Clause_p subsumer, Clause_p sub_candidate)
{
   bool res;

   assert(sub_candidate->weight == ClauseStandardWeight(sub_candidate));
   assert(subsumer->weight == ClauseStandardWeight(subsumer));
   res = clause_subsumes_clause(subsumer, sub_candidate);
   
   if(res)
   {
      DocClauseQuote(GlobalOut, OutputLevel, 6, sub_candidate,
		     "subsumed", subsumer);
      ClauseSetProp(subsumer, ClauseQueryProp(sub_candidate,CPIsSOS));
   }
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: ClauseSetSubsumesClause()
//
//   Return true if the set subsumes sub_candidate. All clauses need
//   correct weights!
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool ClauseSetSubsumesClause(ClauseSet_p set, Clause_p sub_candidate)
{
   if(set->fvindex)
   {
      bool res; 
      FreqVector_p vec = StandardFreqVectorCompute(sub_candidate,
						   set->fvindex->symbol_limit);
      res =  clause_set_subsumes_clause_indexed(set->fvindex->index, vec, 0);
      FreqVectorFree(vec);
      return res;
   }
   return clause_set_subsumes_clause(set, sub_candidate);
}


/*-----------------------------------------------------------------------
//
// Function: ClauseSetFindSubsumedClause()
//
//   Return a pointer to the first clause in the list at or after
//   set_position that is subsumed by the (non-unit)clause
//   subsumer. Return NULL, if no such clause exists.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

Clause_p ClauseSetFindSubsumedClause(ClauseSet_p set, Clause_p
				     set_position, Clause_p
				     subsumer)
{
   assert(subsumer->weight == ClauseStandardWeight(subsumer));

   while(set_position != set->anchor)
   {
      assert(set_position->weight == ClauseStandardWeight(set_position));
      if(clause_subsumes_clause(subsumer, set_position))
      {
	 DocClauseQuote(GlobalOut, OutputLevel, 6, set_position,
			"subsumed", subsumer); 
	 return set_position;
      }
      set_position = set_position->succ;
   }
   return NULL;
}


/*-----------------------------------------------------------------------
//
// Function: ClauseSetFindSubsumedClauses()
//
//   Find all clauses in set that are subsumed by subsumer, and push
//   them onto stack. Return number of clauses found.
//
// Global Variables: 
//
// Side Effects    : 
//
/----------------------------------------------------------------------*/
   
long ClauseSetFindSubsumedClauses(ClauseSet_p set, 
				  FVPackedClause_p subsumer, 
				  PStack_p res)
{
   long old_sp = PStackGetSP(res);

   assert(subsumer->clause->weight == ClauseStandardWeight(subsumer->clause));
   
   if(set->fvindex)
   {
      clauseset_find_subsumed_clauses_indexed(set->fvindex->index,
					      subsumer, 0, res);
   }
   else
   {
      clauseset_find_subsumed_clauses(set, subsumer->clause, res);
   }
   return PStackGetSP(res)-old_sp;
}




/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/


