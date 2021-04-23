// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

// MapleCOMSPS includes
#include "mapleChronoBT/utils/System.h"
#include "mapleChronoBT/core/Dimacs.h"
#include "mapleChronoBT/simp/SimpSolver.h"

#include "../utils/Logger.h"
#include "../utils/System.h"
#include "../utils/Parameters.h"
#include "../clauses/ClauseManager.h"
#include "../solvers/MapleChronoBTSolver.h"
#include <algorithm>

using namespace MapleChronoBT;

// Macros for minisat literal representation conversion
#define MINI_LIT(lit) lit > 0 ? mkLit(lit - 1, false) : mkLit((-lit) - 1, true)

#define INT_LIT(lit) sign(lit) ? -(var(lit) + 1) : (var(lit) + 1)


static void makeMiniVec(ClauseExchange * cls, vec<Lit> & mcls)
{
   for (size_t i = 0; i < cls->size; i++) {
      mcls.push(MINI_LIT(cls->lits[i]));
   }
}


void cbkMapleChronoBTExportClause(void * issuer, int lbd, vec<Lit> & cls)
{
	MapleChronoBTSolver* mp = (MapleChronoBTSolver*)issuer;

	if (lbd > mp->lbdLimit)
		return;

	ClauseExchange * ncls = ClauseManager::allocClause(cls.size());

	for (int i = 0; i < cls.size(); i++) {
		ncls->lits[i] = INT_LIT(cls[i]);
	}

   ncls->lbd  = lbd;
   ncls->from = mp->id;

   mp->clausesToExport.addClause(ncls);
}

Lit cbkMapleChronoBTImportUnit(void * issuer)
{
   MapleChronoBTSolver * mp = (MapleChronoBTSolver*)issuer;

   Lit l = lit_Undef;

   ClauseExchange * cls = NULL;

   if (mp->unitsToImport.getClause(&cls) == false)
      return l;

   l = MINI_LIT(cls->lits[0]);

   ClauseManager::releaseClause(cls);

   return l;
}

bool cbkMapleChronoBTImportClause(void * issuer, int * lbd, vec<Lit> & mcls)
{
   MapleChronoBTSolver* mp = (MapleChronoBTSolver*)issuer;

   ClauseExchange * cls = NULL;

   if (mp->clausesToImport.getClause(&cls) == false)
      return false;

   makeMiniVec(cls, mcls);

   *lbd = cls->lbd;

   ClauseManager::releaseClause(cls);

   return true;
}

MapleChronoBTSolver::MapleChronoBTSolver(int id) : SolverInterface(id, MINISAT)
{
	lbdLimit = Parameters::getIntParam("lbd-limit", 2);

	solver = new SimpSolver();

	solver->cbkExportClause = cbkMapleChronoBTExportClause;
	solver->cbkImportClause = cbkMapleChronoBTImportClause;
	solver->cbkImportUnit   = cbkMapleChronoBTImportUnit;
	solver->issuer          = this;
}

MapleChronoBTSolver::~MapleChronoBTSolver()
{
	delete solver;
}

bool
MapleChronoBTSolver::loadFormula(const char* filename)
{
    gzFile in = gzopen(filename, "rb");

    parse_DIMACS(in, *solver);

    gzclose(in);

    return true;
}

//Get the number of variables of the formula
int
MapleChronoBTSolver::getVariablesCount()
{
	return solver->nVars();
}

// Get a variable suitable for search splitting
int
MapleChronoBTSolver::getDivisionVariable()
{
   return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void
MapleChronoBTSolver::setPhase(const int var, const bool phase)
{
	solver->setPolarity(var - 1, phase ? true : false);
}

// Bump activity for a given variable
void
MapleChronoBTSolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
MapleChronoBTSolver::setSolverInterrupt()
{
   stopSolver = true;

   solver->interrupt();
}

void
MapleChronoBTSolver::unsetSolverInterrupt()
{
   stopSolver = false;

	solver->clearInterrupt();
}

// Diversify the solver
void
MapleChronoBTSolver::diversify(int id)
{
   if (id % 2) {
      solver->LRB = true;
   } else {
      solver->LRB = false;
   }
   if (id % 4 >= 2) {
      solver->verso = false;
   } else {
      solver->verso = true;
   }
   // Suppose at least 8 cores
   if (id % 8 < 4) {
      solver->chrono = -1;
   }
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
MapleChronoBTSolver::solve(const vector<int> & cube)
{
   unsetSolverInterrupt();

   vector<ClauseExchange *> tmp;

   tmp.clear();
   clausesToAdd.getClauses(tmp);

   for (size_t ind = 0; ind < tmp.size(); ind++) {
      vec<Lit> mcls;
      makeMiniVec(tmp[ind], mcls);

      ClauseManager::releaseClause(tmp[ind]);

      if (solver->addClause(mcls) == false) {
         printf("c unsat when adding cls\n");
         return UNSAT;
      }
   }

   vec<Lit> miniAssumptions;
   for (size_t ind = 0; ind < cube.size(); ind++) {
      miniAssumptions.push(MINI_LIT(cube[ind]));
   }

   lbool res = solver->solveLimited(miniAssumptions);

   if (res == l_True)
      return SAT;

   if (res == l_False)
      return UNSAT;

   return UNKNOWN;
}

void
MapleChronoBTSolver::addClause(ClauseExchange * clause)
{
   clausesToAdd.addClause(clause);

   setSolverInterrupt();
}

void
MapleChronoBTSolver::addLearnedClause(ClauseExchange * clause)
{
   if (clause->size == 1) {
      unitsToImport.addClause(clause);
   } else {
      clausesToImport.addClause(clause);
   }
}

void
MapleChronoBTSolver::addClauses(const vector<ClauseExchange *> & clauses)
{
   clausesToAdd.addClauses(clauses);

   setSolverInterrupt();
}

void
MapleChronoBTSolver::addInitialClauses(const vector<ClauseExchange *> & clauses)
{
   for (size_t ind = 0; ind < clauses.size(); ind++) {
      vec<Lit> mcls;

      for (size_t i = 0; i < clauses[ind]->size; i++) {
         int lit = clauses[ind]->lits[i];
         int var = abs(lit);

         while (solver->nVars() < var) {
            solver->newVar();
         }

         mcls.push(MINI_LIT(lit));
      }

      if (solver->addClause(mcls) == false) {
         printf("c unsat when adding initial cls\n");
      }
   }
}

void
MapleChronoBTSolver::addLearnedClauses(const vector<ClauseExchange *> & clauses)
{
   for (size_t i = 0; i < clauses.size(); i++) {
      addLearnedClause(clauses[i]);
   }
}

void
MapleChronoBTSolver::getLearnedClauses(vector<ClauseExchange *> & clauses)
{
   clausesToExport.getClauses(clauses);
}

void
MapleChronoBTSolver::increaseClauseProduction()
{
   lbdLimit++;
}

void
MapleChronoBTSolver::decreaseClauseProduction()
{
   if (lbdLimit > 2) {
      lbdLimit--;
   }
}

SolvingStatistics
MapleChronoBTSolver::getStatistics()
{
   SolvingStatistics stats;

   stats.conflicts    = solver->conflicts;
   stats.propagations = solver->propagations;
   stats.restarts     = solver->starts;
   stats.decisions    = solver->decisions;
   stats.memPeak      = memUsedPeak();

   return stats;
}

std::vector<int>
MapleChronoBTSolver::getModel()
{
   std::vector<int> model;

   for (int i = 0; i < solver->nVars(); i++) {
      if (solver->model[i] != l_Undef) {
         int lit = solver->model[i] == l_True ? i + 1 : -(i + 1);
         model.push_back(lit);
      }
   }

   return model;
}


vector<int>
MapleChronoBTSolver::getFinalAnalysis()
{
   vector<int> outCls;

   for (int i = 0; i < solver->conflict.size(); i++) {
      outCls.push_back(INT_LIT(solver->conflict[i]));
   }

   return outCls;
}

vector<int>
MapleChronoBTSolver::getSatAssumptions()
{
   vector<int> outCls;
   return outCls;
}
