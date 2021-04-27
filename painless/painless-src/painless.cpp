// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
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

#include "painless.h"

#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/System.h"
#include "utils/SatUtils.h"

#include "solvers/SolverFactory.h"
#include "solvers/Reducer.h"
#include "solvers/VirtualSolverSynchronous.h"
#include "solvers/VirtualSolverAsynchronous.h"
#include "solvers/MapleCOMSPSSolver.h"

#include "clauses/ClauseManager.h"

#include "sharing/HordeSatSharing.h"
#include "sharing/Sharer.h"

#include "working/SequentialWorker.h"
#include "working/Portfolio.h"

#include <unistd.h>
#include <cassert>
#include <mpi.h>

using namespace std;

// -------------------------------------------
// Declaration of global variables
// -------------------------------------------
atomic<bool> globalEnding(false);

bool localEnding = false;

Sharer ** sharers = NULL;

int nSharers = 0;

WorkingStrategy * working = NULL;

SatResult finalResult = UNKNOWN;

vector<int> finalModel;

vector<SolverInterface *> solvers;

int MPI_RANK;

int MPI_SIZE; 

// -------------------------------------------
// Main of the framework
// -------------------------------------------
int main(int argc, char ** argv)
{
   // Initializes the MPI execution environment 
   int req = MPI_THREAD_MULTIPLE, prov;
   MPI_Init_thread(&argc, &argv, req, &prov);
   assert(req == prov);

   // Get process' rank and total number of processes lauched
   MPI_Comm_rank(MPI_COMM_WORLD, &MPI_RANK);
   MPI_Comm_size(MPI_COMM_WORLD, &MPI_SIZE);

   // Synchronization between all processes needed to avoid errors
   MPI_Barrier(MPI_COMM_WORLD);

   Parameters::init(argc, argv);

   if (Parameters::getFilename() == NULL || Parameters::getBoolParam("h"))
   {
      if (MPI_RANK == 0) {
         cout << "USAGE: " << argv[0] << " [options] input.cnf" << endl;
         cout << "Options:" << endl;
         cout << "\t-c=<INT>\t\t number of cpus, default is 24" << endl;
         cout << "\t-max-memory=<INT>\t memory limit in GB, default is 51" << \
   	      endl;
         cout << "\t-t=<INT>\t\t timeout in seconds, default is no limit" << endl;
         cout << "\t-lbd-limit=<INT>\t LBD limit of exported clauses, default is" \
   	      " 2" << endl;
         cout << "\t-shr-sleep=<INT>\t time in useconds a sharer sleep each " \
            "round, default is 500000 (0.5s)" << endl;
         cout << "\t-shr-lit=<INT>\t\t number of literals shared per round, " \
            "default is 1500" << endl;
         cout << "\t-ext-shr-strat=<INT>\t 1=synchronous, 2=asynchronous," \
                " default is 0" << endl;
         cout << "\t-v=<INT>\t\t verbosity level, default is 0" << endl;
      }
      // Terminates MPI execution environment
      MPI_Finalize();
      return 0;
   }

   bool allEnding[MPI_SIZE];

   int cpus = Parameters::getIntParam("c", 8);
   setVerbosityLevel(Parameters::getIntParam("v", 0));

   // Create and init solvers
   vector<SolverInterface *> solvers_LRB;
   vector<SolverInterface *> solvers_VSIDS;

   MapleCOMSPS::Community* com = new MapleCOMSPS::Community();
   SolverFactory::createMapleCOMSPSSolvers(cpus - 2, com, solvers);
   MapleCOMSPSSolver* community_manager = (MapleCOMSPSSolver*) solvers[0];
   community_manager->set_community_manager();
   solvers.push_back(SolverFactory::createReducerSolver(SolverFactory::createMapleCOMSPSSolver()));

   int nSolvers = solvers.size();

   SolverFactory::nativeDiversification(solvers);

   for (int id = 0; id < nSolvers; id++) {
      if (id % 2) {
         solvers_LRB.push_back(solvers[id]);
      } else {
         solvers_VSIDS.push_back(solvers[id]);
      }
   }

   SolverFactory::sparseRandomDiversification(solvers_LRB);
   SolverFactory::sparseRandomDiversification(solvers_VSIDS);

   // Init Sharing
   // 15 CDCL, 1 Reducer producers by Sharer
   vector<SolverInterface* > prod1;
   vector<SolverInterface* > prod2;
   vector<SolverInterface* > cons1;
   vector<SolverInterface* > cons2;
   vector<SolverInterface *> from;

   // External sharing managing
   SolverInterface * virtualSolver = NULL;
   switch(Parameters::getIntParam("ext-shr-strat", 2)) {
    case 1 :
      virtualSolver = SolverFactory::createVirtualSolverSynchronous();
      break;
    case 2 :
      virtualSolver = SolverFactory::createVirtualSolverAsynchronous();
      break;
   }

   switch(Parameters::getIntParam("shr-strat", 2)) {
      // Init Sharing
      case 1 :
         prod1.insert(prod1.end(), solvers.begin(), solvers.begin() + (cpus/2 - 1));
         prod1.push_back(solvers[solvers.size() - 2]);
         prod2.insert(prod2.end(), solvers.begin() + (cpus/2 - 1), solvers.end() - 2);
         prod2.push_back(solvers[solvers.size() - 1]);
         // 30 CDCL, 1 Reducer consumers by Sharer
         cons1.insert(cons1.end(), solvers.begin(), solvers.end() - 1);
         cons1.push_back(virtualSolver);
         cons2.insert(cons2.end(), solvers.begin(), solvers.end() - 2);
         cons2.push_back(solvers[solvers.size() - 1]);
         cons2.push_back(virtualSolver);

         nSharers = 2;
         sharers  = new Sharer*[nSharers];
         sharers[0] = new Sharer(1, new HordeSatSharing(), prod1, cons1);
         sharers[1] = new Sharer(2, new HordeSatSharing(), prod2, cons2);
         break;
      case 2:
         prod1.insert(prod1.end(), solvers.begin(), solvers.end());
         prod1.push_back(virtualSolver);
         nSharers = 1;
         sharers  = new Sharer*[nSharers];
         sharers[0] = new Sharer(1, new HordeSatSharing(), prod1, prod1);
   }

   if (virtualSolver) {
      solvers.push_back(virtualSolver);  
   }

   // Init working
   working = new Portfolio();
   for (size_t i = 0; i < nSolvers; i++) {
      working->addSlave(new SequentialWorker(solvers[i]));
   }

   // Init the management of clauses
   ClauseManager::initClauseManager();

   // Launch working
   vector<int> cube;
   working->solve(cube);

   // Wait until end or timeout
   int timeout = Parameters::getIntParam("t", -1);
   int winning_node;
   MPI_Request modelRequest;

   while(true) {
      sleep(1);
      winning_node = 0;
      localEnding = globalEnding.load();

      MPI_Allgather(
         &localEnding,
         1,
         MPI_C_BOOL,
         allEnding,
         1,
         MPI_C_BOOL,
         MPI_COMM_WORLD
      );
      while (winning_node < MPI_SIZE && !allEnding[winning_node]) {
         winning_node++;
      }
      if (winning_node < MPI_SIZE) {
         globalEnding = true;
	 //working->setInterrupt();
         break;
      }

      if (timeout > 0 && getRelativeTime() >= timeout) {
         globalEnding = true;
         working->setInterrupt();
      }
   }
   working->setInterrupt();

   // Delete sharers
  for (int id = 0; id < nSharers; id++) {
      // if (MPI_RANK == winning_node) {
         // sharers[id]->printStats();
      // }
      delete sharers[id];
   }
  delete sharers;

   // Print solver stats
   // if (MPI_RANK == winning_node) {
   //    SolverFactory::printStats(solvers);
   // }

   // Delete working strategy
   delete working;
   
   //Delete virtual solver
   delete virtualSolver;

   // Delete shared clauses
   ClauseManager::joinClauseManager();
   
   // Print the result and the model if SAT
   if (MPI_RANK == winning_node) {
      vector<int> endModel = finalModel;
      SatResult endResult = finalResult;

      // cout << "c Resolution time: " << getRelativeTime() << "s" << endl;

      if (endResult == SAT) {
         cout << "s SATISFIABLE" << endl;

         if (Parameters::getBoolParam("no-model") == false) {
            printModel(endModel);
         }
      } else if (endResult == UNSAT) {
         cout << "s UNSATISFIABLE" << endl;
      } else {
         cout << "s UNKNOWN" << endl;
      }
      // MPI_Abort(MPI_COMM_WORLD, 0);
   }
   MPI_Finalize();
   return 0;
}
