/**
 * Copyright (c) 2016      Los Alamos National Security, LLC
 *                         All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * LA-CC 10-123
 */

//@HEADER
// ***************************************************
//
// HPCG: High Performance Conjugate Gradient Benchmark
//
// Contact:
// Michael A. Heroux ( maherou@sandia.gov)
// Jack Dongarra     (dongarra@eecs.utk.edu)
// Piotr Luszczek    (luszczek@eecs.utk.edu)
//
// ***************************************************
//@HEADER

/**
 *
 */

#include <iostream>
#include <cstdlib>
#include <deque>

#include "hpcg.hpp"
#include "mytimer.hpp"
#include "Geometry.hpp"
#include "CheckAspectRatio.hpp"
#include "GenerateGeometry.hpp"

#include "LegionStuff.hpp"
#include "LogicalArray.hpp"
#include "CGMapper.h"

using namespace std;

LegionRuntime::Logger::Category Logger("LGNCG");

/**
 * SPMD Main Task //////////////////////////////////////////////////////////////
 */
void
spmdMainTask(
    const Task *task,
    const std::vector<PhysicalRegion> &regions,
    Context ctx, HighLevelRuntime *runtime
) {
#if 0
    const SPMDContext &spmdCtx = *(const SPMDContext *)(task->args);
    //
    HPCG_Params params;
    SPMDMeta spmdMeta = {.nRanks = nShards /* One rank/shard (for now) */};
    HPCG_Init(params, spmdMeta);
    // Check if QuickPath option is enabled.  If the running time is set to
    // zero, we minimize all paths through the program
    bool quickPath = (params.runningTime == 0);
    // Number of processes, my process ID
    int size = params.comm_size, rank = params.comm_rank;
    //
    local_int_t nx, ny,nz;
    nx = (local_int_t)params.nx;
    ny = (local_int_t)params.ny;
    nz = (local_int_t)params.nz;
    // Used to check return codes on function calls
    int ierr = 0;
    ierr = CheckAspectRatio(0.125, nx, ny, nz, "local problem", rank == 0);
    if (ierr) exit(ierr);
    ////////////////////////////////////////////////////////////////////////////
    // Problem setup Phase /////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // Construct the geometry and linear system
    Geometry *geom = new Geometry;
    GenerateGeometry(size, rank, params.numThreads, nx, ny, nz, geom);

    ierr = CheckAspectRatio(0.125, geom->npx, geom->npy, geom->npz,
                            "process grid", rank == 0);
    if (ierr) exit(ierr);

    // Use this array for collecting timing information
    std::vector< double > times(10,0.0);

    double setup_time = mytimer();
#endif
}

/**
 * Main Task ///////////////////////////////////////////////////////////////////
 * First task that gets spawned. Responsible for setup, etc.
 */
void
mainTask(
    const Task *task,
    const std::vector<PhysicalRegion> &regions,
    Context ctx, HighLevelRuntime *runtime
) {
    // ask the mapper how many shards we can have
    int nShards = runtime->get_tunable_value(ctx, CGMapper::TID_NUM_SHARDS);
    cout << "*** Number of Shards (~ NUMPE): " << nShards << endl;;
    ////////////////////////////////////////////////////////////////////////////
    cout << "*** Starting Initialization..." << endl;;
    // Legion array holding HPCG parameters that impact run.
    LogicalArray<HPCG_Params> hpcgParams;
    hpcgParams.allocate(nShards, ctx, runtime);
    // Legion array holding problem geometries.
    LogicalArray<Geometry> geometries;
    hpcgParams.allocate(nShards, ctx, runtime);
    //
    cout << "*** Launching Initialization Tasks..." << endl;;
    const auto launchDomain = Domain::from_rect<1>(Rect<1>(0, nShards - 1));
    IndexLauncher launcher(
        SPMD_INIT_TID,
        launchDomain,
        TaskArgument(nullptr, 0),
        ArgumentMap()
    );
    launcher.add_region_requirement(
        RegionRequirement(
            hpcgParams.logicalPartition(),
            0,
            WRITE_DISCARD,
            EXCLUSIVE,
            hpcgParams.logicalRegion
        )
    );
    // TODO add fields...
    auto futureMap = runtime->execute_index_space(ctx, launcher);
    futureMap.wait_all_results();
    cout << "*** Waiting for Initialization Tasks" << endl;

    cout << "*** Cleaning Up..." << endl;
    hpcgParams.deallocate(ctx, runtime);
}

/**
 * Main ////////////////////////////////////////////////////////////////////////
 * Responsible for RT initialization.
 */
int
main(int argc, char **argv)
{
    LegionInit();
    return Runtime::start(argc, argv);
}
