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

#include "../solvers/VirtualSolverAsynchronous.h"

#include "../clauses/ClauseManager.h"
#include "../clauses/ClauseExchange.h"

#include "../utils/Parameters.h"

#include "../sharing/HordeSatSharing.h"
#include "../sharing/Sharer.h"

#include <unistd.h>
#include <cassert>
#include <vector>
#include <mpi.h>

using namespace std;

// Main executed by import thread
static void * mainVirtualSolverImport(void * arg)
{
   VirtualSolverAsynchronous * vsolver  = (VirtualSolverAsynchronous *) arg;
   int sleepTime = Parameters::getIntParam("shr-sleep", 500000);

   // Array for receiving
   int * importBuffer = NULL;

   // Other variables
   int flag;
   MPI_Status status;
   int bufsize, bufcounter;
   vector<ClauseExchange *> tmp;

   while (!vsolver->externalEnding) {
      /* 1 - Wait for and incoming message (TAG == 0 for shared clauses)*/
      MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
      if (!flag) {
         // Sleep if no incoming message
         usleep(sleepTime);
         continue;
      }

      /* 2 - Get the amount of data exchanged */
      MPI_Get_count(&status, MPI_INT, &bufsize);

      /* 3 - Allocation memory */
      importBuffer = (int*) realloc(importBuffer, bufsize * sizeof(int));
      assert(importBuffer);

      /* 4 - Receive the data */
      MPI_Recv(importBuffer,
               bufsize,
               MPI_INT,
               status.MPI_SOURCE,
               status.MPI_TAG,
               MPI_COMM_WORLD,
               MPI_STATUS_IGNORE
      );

      /* 5 - Manage the incomming data */
      bufcounter = 0;
      
      // Normal clause message
      tmp.clear();
      while (bufcounter < bufsize) {
         int size = importBuffer[bufcounter++];
         ClauseExchange *nclause = ClauseManager::allocClause(size);
         nclause->lbd = importBuffer[bufcounter++];
         nclause->from = -1; // value never used
         for (int i = 0; i < size; i++) {
            nclause->lits[i] = importBuffer[bufcounter++];
         }
         tmp.push_back(nclause);
         vsolver->nbClausesImported++;
      }
      vsolver->clausesToImport.addClauses(tmp);
   }

   free(importBuffer);
   //cout << "Import of rank " << MPI_RANK << " ending" << endl;
   return NULL;
}

// Main executed by export thread
static void * mainVirtualSolverExport(void * arg)
{
   VirtualSolverAsynchronous * vsolver  = (VirtualSolverAsynchronous *) arg;
   
   // Other variables
   int flag;
   int bufsize, bufcounter;
   int reqcounter;
   int selectCount;
   vector<ClauseExchange *> tmp;
   int literalPerRound = Parameters::getIntParam("shr-lit", 1500);

   while (!vsolver->externalEnding) {
      pthread_mutex_lock(&vsolver->mutexFull);
      while (vsolver->clausesToExport.size() == 0 && !vsolver->externalEnding) {
         // Wait until there are clauses to export or the end
         // is reached (by oneself or by an external process)
         pthread_cond_wait(&vsolver->mutexCondFull, &vsolver->mutexFull);
      }
      pthread_mutex_unlock(&vsolver->mutexFull);
      if (vsolver->externalEnding)
         break;

      /* 1 - Pop the clauses to be exported (if not globalEnding) */
      tmp.clear();
      vsolver->clausesToExport.getClauses(tmp);
      for (size_t i = 0; i < tmp.size(); i++) {
         if (tmp[i]->lbd > 2) {
            ClauseManager::releaseClause(tmp[i]);
            continue;
         }
         vsolver->dbToExport.addClause(tmp[i]);
      }
      tmp.clear();
      vsolver->dbToExport.giveSelection(tmp, literalPerRound, &selectCount);
      if (selectCount == 0)
         continue;
      /* 2 - Compute buffer's size and allocate memory 
             * If learned clauses, then, for each clause: [size | lbd | lits]
             * If termination information, then: [finalResult | finalModel.size() | finalModel] */
      bufsize = 0;

      for (ClauseExchange *clause : tmp) {
         bufsize += /* size + lbd */ 2 + /* lits */ clause->size;
         vsolver->nbClausesExported++;
      }
      // /!\ Before modifying exportBuffer, one has to wait the completion 
      // of a previous sending (or the reception of a termination result)
      flag = false;
      while (!flag && !vsolver->externalEnding) {
         MPI_Testall(MPI_SIZE - 1, vsolver->exportRequests, &flag, MPI_STATUSES_IGNORE);
      }
      if (vsolver->externalEnding)
         break;
      
      vsolver->exportBuffer = (int*) realloc(vsolver->exportBuffer, bufsize * sizeof(int)); 
      assert(vsolver->exportBuffer);

      /* 3 - Fill the buffer */
      bufcounter = 0;

      // Normal clause message
      for (ClauseExchange *clause : tmp) {

         vsolver->exportBuffer[bufcounter++] = clause->size;
         vsolver->exportBuffer[bufcounter++] = clause->lbd;
         for (int i = 0; i < clause->size; i++) {
           vsolver->exportBuffer[bufcounter++] = clause->lits[i];
         }
         ClauseManager::releaseClause(clause);
      }

      /* 4 - Send data to all the other processes */
      reqcounter = 0;
      for(int i = 0; i < MPI_SIZE; i++) {
         if (i == MPI_RANK) { // Don't send to the current process
            continue;
         }

         MPI_Isend(
            vsolver->exportBuffer,
            bufsize,
            MPI_INT,
            i,
            0, //TAG == 0 for shared clauses
            MPI_COMM_WORLD,
            &vsolver->exportRequests[reqcounter++]
         );
         
      }
      
   }
   //cout << "Export of rank " << MPI_RANK << " ending " << endl;
   return NULL;
}

// Constructor
VirtualSolverAsynchronous::VirtualSolverAsynchronous(int id) : 
   VirtualSolver(id, VIRTUAL_SOLVER_ASYNCHRONOUS)
{
   externalEnding = false;

   exportRequests = (MPI_Request *) malloc((MPI_SIZE - 1) * sizeof(MPI_Request));
   for (int i = 0; i < MPI_SIZE - 1; i++) {
      exportRequests[i] = MPI_REQUEST_NULL;
   }
   exportBuffer = NULL;

   pthread_mutex_init(&mutexFull, NULL);
   pthread_cond_init(&mutexCondFull, NULL);

   threadImport = new Thread(mainVirtualSolverImport, this);
   threadExport = new Thread(mainVirtualSolverExport, this);
   nbClausesExported = 0;
   nbClausesImported = 0;
}

// Destructor
VirtualSolverAsynchronous::~VirtualSolverAsynchronous()
{  
   int flag;
   for (int i = 0; i < MPI_SIZE - 1; i++) {
      MPI_Test(&exportRequests[i], &flag, MPI_STATUS_IGNORE);
      if (!flag) {
         MPI_Cancel(&exportRequests[i]);
         MPI_Request_free(&exportRequests[i]);
      }
   }
   externalEnding = true;
   //threadImport->cancel();
   threadImport->join();

   // Signal if the export thread is blocked
   pthread_mutex_lock  (&mutexFull);
   pthread_cond_signal (&mutexCondFull);
   pthread_mutex_unlock(&mutexFull);

   //threadExport->cancel();
   threadExport->join();

   delete threadImport;
   delete threadExport;

   free(exportRequests);
   free(exportBuffer);

   pthread_mutex_destroy(&mutexFull);
   pthread_cond_destroy(&mutexCondFull);
}

void VirtualSolverAsynchronous::addLearnedClauses(const vector<ClauseExchange *> & clauses)
{
   clausesToExport.addClauses(clauses);
   pthread_mutex_lock(&mutexFull);
   pthread_cond_signal(&mutexCondFull);
   pthread_mutex_unlock(&mutexFull);
}

SolvingStatistics
VirtualSolverAsynchronous::getStatistics()
{
   SolvingStatistics s;
   s.conflicts = nbClausesExported;
   s.propagations = nbClausesImported;

   return s;
}
