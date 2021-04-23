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
#include "../painless.h"
#include "../solvers/VirtualSolverSynchronous.h"
#include "../utils/Parameters.h"
#include "../clauses/ClauseManager.h"
#include "../clauses/ClauseExchange.h"

#include <unistd.h>
#include <cassert>
#include <vector>
#include <mpi.h>

using namespace std;

// Main executed by virtual solver thread
static void * mainVirtualSolver(void * arg)
{
   VirtualSolverSynchronous * vsolver  = (VirtualSolverSynchronous *) arg;
   int sleepTime = Parameters::getIntParam("shr-sleep", 500000);

   // Arrays needed for the exchange
   int * sendbuf    = NULL;
   int * recvbuf    = NULL;
   int * recvcounts = (int*) malloc(MPI_SIZE * sizeof(int));  // Number of integers received
   int * rdispls    = (int*) malloc(MPI_SIZE * sizeof(int));  // Displacements for the reception
   int * sendcounts = (int*) malloc(MPI_SIZE * sizeof(int));  // Number of integers sent
   int * sdispls    = (int*) calloc(MPI_SIZE, sizeof(int));   // Displacements for the sending
   assert(recvcounts);
   assert(rdispls);
   assert(sendcounts);
   assert(sdispls);

   // Other variables
   bool localEnding;
   vector<ClauseExchange *> tmp;
   int localcount, recvbufsize, sendbufcounter;

   while (true) {
      // Sleep 
      usleep(sleepTime);

      // As the variable globalEnding can be modified by another thread,
      // its value is fixed at the beginning of each iteration
      localEnding = globalEnding.load();

      /* 1 - Pop the clauses to be exported (if not globalEnding) */
      tmp.clear();
      if (!localEnding) {
         vsolver->clausesToExport.getClauses(tmp);
      }

      /* 2 - Compute the size of the data to be exported
             * If learned clauses, then, for each clause: [size | lbd | lits]
             * If termination information, then: [-1 | finalResult | finalModel.size() | finalModel] */
      localcount = 0;
      for (ClauseExchange *clause : tmp) {
         localcount +=  /* size + lbd */ 2 + /* lits */ clause->size;
      }
      if (localEnding) {
         localcount = /* integer -1 */ 1 + /* finalResult */ 1 + 
                      /* finalModel.size()*/ 1 + /* finalModel */ finalModel.size(); 
      }

      /* 3 - First exchange phase (the size of the data) */
      MPI_Allgather(
         &localcount,
         1,
         MPI_INT,
         recvcounts,
         1,
         MPI_INT,
         MPI_COMM_WORLD
      );

      recvcounts[MPI_RANK] = 0; // don't receive data from the current rank

      /* 4 - Compute the size of recvbuf (+ allocation)
           - Fill rdispls (displacements for the reception) 
           - Fill sendcounts (number of integers to be sent to each process) */
      recvbufsize = 0; 
      for (int i = 0; i < MPI_SIZE; i++) {
         rdispls[i] = recvbufsize;
         recvbufsize += recvcounts[i];
         sendcounts[i] = localcount;
      }
      recvbuf = (int*) realloc(recvbuf, recvbufsize*sizeof(int));
      assert(recvbuf);
      sendcounts[MPI_RANK] = 0; // don't send data to the current rank

      /* 5 - Allocate and fill the sending buffer */
      sendbuf = (int*) realloc(sendbuf, localcount*sizeof(int));
      assert(sendbuf);
      sendbufcounter = 0;
      for (ClauseExchange *clause : tmp) {
         sendbuf[sendbufcounter++] = clause->size;
         sendbuf[sendbufcounter++] = clause->lbd;
         for (int i = 0; i < clause->size; i++) {
            sendbuf[sendbufcounter++] = clause->lits[i];
         }
         ClauseManager::releaseClause(clause);
      }
      if (localEnding) {
         sendbuf[sendbufcounter++] = -1;
         sendbuf[sendbufcounter++] = finalResult;
         sendbuf[sendbufcounter++] = finalModel.size();
         for (int i = 0; i < finalModel.size(); i++) {
            sendbuf[sendbufcounter++] = finalModel[i];
         } 
      }

      /* 6 - Second exchange phase (learned clauses or termination messages) */
      MPI_Alltoallv(
         sendbuf,
         sendcounts,
         sdispls,
         MPI_INT,
         recvbuf,
         recvcounts,
         rdispls,
         MPI_INT,
         MPI_COMM_WORLD
      );

      // If localEnding, there is nothing more to be done
      if (localEnding) {
         break;
      }

      /* 7 - Manage incoming data */
      tmp.clear();
      for (int i = 0; i < MPI_SIZE; i++) {
         if (recvcounts[i] == 0) { // the process didn't send data
            continue;
         }
         int j = 0;
         while (j < recvcounts[i]) {
            int size = recvbuf[rdispls[i] + j++];
            // Special termination message
            if (size == -1) {
               finalResult = (SatResult) recvbuf[rdispls[i] + j++];
               int finalModelSize = recvbuf[rdispls[i] + j++];
               for (int k = 0; k < finalModelSize; k++) {
                  finalModel.push_back(recvbuf[rdispls[i] + j++]);
               }
               globalEnding = true;
               goto FINISHED;
            }
            // Normal clause message
            ClauseExchange *nclause = ClauseManager::allocClause(size);
            nclause->lbd = recvbuf[rdispls[i] + j++];
            nclause->from = -1; // value never used
            for (int k = 0; k < size; k++) {
               nclause->lits[k] = recvbuf[rdispls[i] + j++];
            }
            tmp.push_back(nclause);
         }
      }

      vsolver->clausesToImport.addClauses(tmp);
   }

   FINISHED:
   free(sendbuf);
   free(recvbuf);
   free(recvcounts);
   free(rdispls);
   free(sendcounts);
   free(sdispls);
   return NULL;
}

// Constructor
VirtualSolverSynchronous::VirtualSolverSynchronous(int id) : VirtualSolver(id, VIRTUAL_SOLVER_SYNCHRONOUS)
{
   thread = new Thread(mainVirtualSolver, this);
}

// Destructor
VirtualSolverSynchronous::~VirtualSolverSynchronous()
{
   thread->join();

   delete thread;
}
