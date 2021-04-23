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

// Main executed by virtual solver thread
static void * mainVirtualSolver(void * arg);

class VirtualSolverSynchronous : public VirtualSolver
{
public:
   VirtualSolverSynchronous(int id);
   
   ~VirtualSolverSynchronous(); 

protected:
   friend void * mainVirtualSolver(void * arg);

   Thread * thread;   
};
