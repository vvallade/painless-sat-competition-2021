// -----------------------------------------------------------------------------
// Copyright (C) 2020  Razvan OANEA
// Télécom Paris, Palaiseau, France
// razvan.oanea@telecom-paris.fr
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

#include "../solvers/VirtualSolver.h"

using namespace std;

VirtualSolver::VirtualSolver(int id, SolverType type) : SolverInterface(id, type)
{
}


VirtualSolver::~VirtualSolver()
{
}

bool
VirtualSolver::loadFormula(const char* filename)
{
}

//Get the number of variables of the formula
int
VirtualSolver::getVariablesCount()
{
}

// Get a variable suitable for search splitting
int
VirtualSolver::getDivisionVariable()
{
}

// Set initial phase for a given variable
void
VirtualSolver::setPhase(const int var, const bool phase)
{
}

// Bump activity for a given variable
void
VirtualSolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
VirtualSolver::setSolverInterrupt()
{
}

void
VirtualSolver::unsetSolverInterrupt()
{
}

// Diversify the solver
void
VirtualSolver::diversify(int id)
{
}

SatResult 
VirtualSolver::solve(const vector<int> & cube)
{
}

void
VirtualSolver::addClause(ClauseExchange * clause)
{
}

void
VirtualSolver::addClauses(const vector<ClauseExchange *> & clauses)
{
}

void
VirtualSolver::addInitialClauses(const vector<ClauseExchange *> & clauses)
{
}

void
VirtualSolver::addLearnedClause(ClauseExchange * clause)
{
}

void VirtualSolver::addLearnedClauses(const vector<ClauseExchange *> & clauses)
{
   clausesToExport.addClauses(clauses);
}

void
VirtualSolver::getLearnedClauses(vector<ClauseExchange *> & clauses)
{
   clausesToImport.getClauses(clauses);
}

void
VirtualSolver::increaseClauseProduction()
{
}

void
VirtualSolver::decreaseClauseProduction()
{
}

SolvingStatistics
VirtualSolver::getStatistics()
{
   SolvingStatistics s;
   return s;
}

vector<int>
VirtualSolver::getModel()
{
   return vector<int>();
}

vector<int>
VirtualSolver::getFinalAnalysis()
{
   return vector<int>();
}

vector<int>
VirtualSolver::getSatAssumptions() {
   return vector<int>();
};