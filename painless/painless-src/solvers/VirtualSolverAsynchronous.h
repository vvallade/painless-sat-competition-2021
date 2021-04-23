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

#pragma once

#include "../solvers/VirtualSolver.h"
#include "../utils/Threading.h"
#include "../clauses/ClauseDatabase.h"

#include <mpi.h>

// Main executed by import thread
static void * mainVirtualSolverImport(void * arg);

// Main executed by export thread
static void * mainVirtualSolverExport(void * arg);

class VirtualSolverAsynchronous : public VirtualSolver
{
public:
   VirtualSolverAsynchronous(int id);
   
   ~VirtualSolverAsynchronous();

   /// Add a list of learned clauses to the formula.
   void addLearnedClauses(const vector<ClauseExchange *> & clauses) override;  

   // Keeping track of imports and exports
   int nbClausesImported;
   int nbClausesExported;

   SolvingStatistics getStatistics() override;

protected:
   friend void * mainVirtualSolverImport(void * arg);
   friend void * mainVirtualSolverExport(void * arg);

   Thread *threadImport;
   Thread *threadExport;

   /// Indicates reception of a termination message from another process
   atomic<bool> externalEnding;

   ClauseDatabase dbToExport;

   /// Buffer for export
   int * exportBuffer;

   /// Requests for export
   MPI_Request * exportRequests;

   /// Request for the non-blocking barrier
   MPI_Request barrierRequest;

   pthread_mutex_t mutexFull;
   pthread_cond_t  mutexCondFull;
};
