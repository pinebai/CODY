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

/*!
 @file GenerateProblem_ref.hpp

 HPCG routine
 */

#pragma once

#include "hpcg.hpp"

#include "LegionItems.hpp"
#include "LegionArrays.hpp"
#include "LegionMatrices.hpp"

// For STL serialization into regions.
#include "cereal/archives/binary.hpp"
#include "cereal/types/map.hpp"

#include <map>
#include <sstream>
#include <cassert>

/*!
    Reference version of GenerateProblem to generate the sparse matrix, right
    hand side, initial guess, and exact solution.

    @param[in]  A        The known system matrix
    @param[inout] b      The newly allocated and generated right hand side
    vector (if b!=0 on entry)
    @param[inout] x      The newly allocated solution vector with entries set to
    0.0 (if x!=0 on entry)
    @param[inout] xexact The newly allocated solution vector with entries set to
    the exact solution (if the xexact!=0 non-zero on entry)

    @see GenerateGeometry
*/
inline void
GenerateProblem(
    SparseMatrix &A,
    Array<floatType> *b,
    Array<floatType> *x,
    Array<floatType> *xexact,
    LegionRuntime::HighLevel::Context ctx,
    LegionRuntime::HighLevel::Runtime *runtime
) {
    using namespace std;
    // Make local copies of geometry information.  Use global_int_t since the
    // RHS products in the calculations below may result in global range values.
    global_int_t nx  = A.geom->nx;
    global_int_t ny  = A.geom->ny;
    global_int_t nz  = A.geom->nz;
    global_int_t npx = A.geom->npx;
    global_int_t npy = A.geom->npy;
    global_int_t npz = A.geom->npz;
    global_int_t ipx = A.geom->ipx;
    global_int_t ipy = A.geom->ipy;
    global_int_t ipz = A.geom->ipz;
    global_int_t gnx = nx*npx;
    global_int_t gny = ny*npy;
    global_int_t gnz = nz*npz;
    // This is the size of our subblock
    local_int_t localNumberOfRows = nx*ny*nz;
    // If this assert fails, it most likely means that the local_int_t is set to
    // int and should be set to.  Throw an exception of the number of rows is
    // less than zero (can happen if int overflow)long long
    assert(localNumberOfRows > 0);
    // We are approximating a 27-point finite element/volume/difference 3D
    // stencil
    local_int_t numberOfNonzerosPerRow = 27;

    // Total number of grid points in mesh
    global_int_t totalNumberOfRows = ((global_int_t)localNumberOfRows)
                                   * ((global_int_t)A.geom->size);
    // If this assert fails, it most likely means that the global_int_t is set
    // to int and should be set to long long
    assert(totalNumberOfRows>0);
    ////////////////////////////////////////////////////////////////////////////
    // Allocate arrays
    ////////////////////////////////////////////////////////////////////////////
    // TODO add vector stats
    if (A.geom->rank == 0) {
        const size_t mn = localNumberOfRows * numberOfNonzerosPerRow;
        const size_t sparseMatMemInB = (
            sizeof(char)         * localNumberOfRows //nonzerosInRow
          + sizeof(global_int_t) * mn                //mtxIndG
          + sizeof(local_int_t)  * mn                //mtxIndL
          + sizeof(floatType)    * mn                //matrixValues
          + sizeof(floatType)    * localNumberOfRows //matrixDiagonal
          + sizeof(global_int_t) * localNumberOfRows //localToGlobalMap
        ) * A.geom->size;
        //
        const size_t vectorsMemInB = (
            (b      ? sizeof(floatType) * localNumberOfRows : 0)
          + (x      ? sizeof(floatType) * localNumberOfRows : 0)
          + (xexact ? sizeof(floatType) * localNumberOfRows : 0)
        ) * A.geom->size;
        //
        const size_t pMemInB = sparseMatMemInB + vectorsMemInB;
        const double pMemInMB = double(pMemInB)/1024/1024;
        cout << "*** Approx. Generate Problem Memory Footprint="
             << pMemInMB << "MB" << endl;
    }
    //
    ArrayAllocator<char> aaNonzerosInRow (
        localNumberOfRows, WO_E, ctx, runtime
    );
    char *nonzerosInRow = aaNonzerosInRow.data();
    assert(nonzerosInRow);
    // Interpreted as 2D array
    ArrayAllocator<global_int_t> aaMtxIndG(
        localNumberOfRows * numberOfNonzerosPerRow, WO_E, ctx, runtime
    );
    global_int_t *mtxIndG1D = aaMtxIndG.data();
    assert(mtxIndG1D);
    // Interpreted as 2D array
    Array2D<global_int_t> mtxIndG(
        localNumberOfRows, numberOfNonzerosPerRow, mtxIndG1D
    );
    // Interpreted as 2D array
    ArrayAllocator<local_int_t> aaMtxIndL(
        localNumberOfRows * numberOfNonzerosPerRow, WO_E, ctx, runtime
    );
    local_int_t *mtxIndL = aaMtxIndL.data();
    assert(mtxIndL);
    ArrayAllocator<floatType> aaMatrixValues(
        localNumberOfRows * numberOfNonzerosPerRow, WO_E, ctx, runtime
    );
    floatType *matrixValues1D = aaMatrixValues.data();
    assert(matrixValues1D);
    // Interpreted as 2D array
    Array2D<floatType> matrixValues(
        localNumberOfRows, numberOfNonzerosPerRow, matrixValues1D
    );
    // Interpreted as 1D array
    ArrayAllocator<floatType> aaMatrixDiagonal(
        localNumberOfRows, WO_E, ctx, runtime
    );
    floatType *matrixDiagonal = aaMatrixDiagonal.data();
    assert(matrixDiagonal);
    // Local-to-global mapping
    ArrayAllocator<global_int_t> aaLocalToGlobalMap(
        localNumberOfRows, WO_E, ctx, runtime
    );
    global_int_t *localToGlobalMap = aaLocalToGlobalMap.data();
    assert(localToGlobalMap);
    //
    floatType *bv      = nullptr;
    floatType *xv      = nullptr;
    floatType *xexactv = nullptr;
    if (b != 0) {
        bv = b->data(); // Only compute exact solution if requested
        assert(bv);
    }
    if (x != 0) {
        xv = x->data(); // Only compute exact solution if requested
        assert(xv);
    }
    if (xexact != 0) {
        xexactv = xexact->data(); // Only compute exact solution if requested
        assert(xexactv);
    }
    //!< global-to-local mapping
    auto &globalToLocalMap = A.globalToLocalMap;
    //
    local_int_t localNumberOfNonzeros = 0;
    //
    for (local_int_t iz=0; iz<nz; iz++) {
        global_int_t giz = ipz*nz+iz;
        for (local_int_t iy=0; iy<ny; iy++) {
            global_int_t giy = ipy*ny+iy;
            for (local_int_t ix=0; ix<nx; ix++) {
                global_int_t gix = ipx*nx+ix;
                local_int_t currentLocalRow = iz*nx*ny+iy*nx+ix;
                global_int_t currentGlobalRow = giz*gnx*gny+giy*gnx+gix;
                globalToLocalMap[currentGlobalRow] = currentLocalRow;
                localToGlobalMap[currentLocalRow] = currentGlobalRow;
                char numberOfNonzerosInRow = 0;
                // Current index in current row
                global_int_t currentIndexG = 0;
                local_int_t currentNonZeroElemIndex = 0;
                for (int sz=-1; sz<=1; sz++) {
                    if (giz+sz>-1 && giz+sz<gnz) {
                        for (int sy=-1; sy<=1; sy++) {
                            if (giy+sy>-1 && giy+sy<gny) {
                                for (int sx=-1; sx<=1; sx++) {
                                    if (gix+sx>-1 && gix+sx<gnx) {
                                        global_int_t curcol = currentGlobalRow
                                                            + sz*gnx*gny
                                                            + sy*gnx+sx;
                                        if (curcol==currentGlobalRow) {
                                            matrixDiagonal[currentLocalRow] = 26.0;
                                            matrixValues(currentLocalRow, currentNonZeroElemIndex) = 26.0;
                                        } else {
                                            matrixValues(currentLocalRow, currentNonZeroElemIndex) = -1.0;
                                        }
                                        currentNonZeroElemIndex++;
                                        mtxIndG(currentLocalRow, currentIndexG++) = curcol;
                                        numberOfNonzerosInRow++;
                                    } // end x bounds test
                                } // end sx loop
                            } // end y bounds test
                        } // end sy loop
                    } // end z bounds test
                } // end sz loop
                nonzerosInRow[currentLocalRow] = numberOfNonzerosInRow;
                localNumberOfNonzeros += numberOfNonzerosInRow;
                if (b!=0) {
                    bv[currentLocalRow] = 26.0
                                        - ((double)(numberOfNonzerosInRow-1));
                }
                if (x!=0) {
                    xv[currentLocalRow] = 0.0;
                }
                if (xexact!=0) {
                    xexactv[currentLocalRow] = 1.0;
                }
            } // end ix loop
        } // end iy loop
    } // end iz loop
    //
    global_int_t totalNumberOfNonzeros = 0;
#if 0
    // SKG TODO calculate totalNumberOfNonzeros
    // Use MPI's reduce function to sum all nonzeros
    // convert to 64 bit for MPI call
    long long lnnz = localNumberOfNonzeros, gnnz = 0;
    MPI_Allreduce(&lnnz, &gnnz, 1, MPI_LONG_LONG_INT, MPI_SUM, MPI_COMM_WORLD);
    totalNumberOfNonzeros = gnnz; // Copy back
#else
    totalNumberOfNonzeros = localNumberOfNonzeros;
#endif
    // If this assert fails, it most likely means that the global_int_t is set
    // to int and should be set to long long This assert is usually the first to
    // fail as problem size increases beyond the 32-bit integer range.  Throw an
    // exception of the number of nonzeros is less than zero (can happen if int
    // overflow)
    assert(totalNumberOfNonzeros>0);
    // Perform serialization of items for storage into logical regions.
    std::stringstream *ssGlobalToLocalMap = new std::stringstream;
    {   // Scoped to guarantee flushing, etc.
        cereal::BinaryOutputArchive oa(*ssGlobalToLocalMap);
        oa(globalToLocalMap);
    }
    // Get size of serialized buffer.
    ssGlobalToLocalMap->seekp(0, ios::end);
    auto regionSizeInB = ssGlobalToLocalMap->tellp();
    //
    string strBuff(ssGlobalToLocalMap->str());
    // Remove one copy of the data, since it is stored elsewhere now.
    delete ssGlobalToLocalMap;
    // Allocate region-based backing store for serialized data.
    ArrayAllocator<char> aaGlobalToLocalMap(
        regionSizeInB, WO_E, ctx, runtime
    );
    char *globalToLocalMapBytesPtr = aaGlobalToLocalMap.data();
    assert(globalToLocalMapBytesPtr);
    // Write back into logical region
    memmove(globalToLocalMapBytesPtr, strBuff.c_str(), regionSizeInB);
    // Kill last unneeded intermediate buffer.
    strBuff.clear();
    // Convenience pointer to A.localData
    auto *AlD = A.localData;
    AlD->maxNonzerosPerRow     = numberOfNonzerosPerRow;
    AlD->totalNumberOfRows     = totalNumberOfRows;
    AlD->totalNumberOfNonzeros = totalNumberOfNonzeros;
    AlD->localNumberOfRows     = localNumberOfRows;
    AlD->localNumberOfColumns  = localNumberOfRows;
    AlD->localNumberOfNonzeros = localNumberOfNonzeros;
    //
       aaNonzerosInRow.bindToLogicalRegion(*(A.pic.nonzerosInRow.data()));
             aaMtxIndG.bindToLogicalRegion(*(A.pic.mtxIndG.data()));
             aaMtxIndL.bindToLogicalRegion(*(A.pic.mtxIndL.data()));
        aaMatrixValues.bindToLogicalRegion(*(A.pic.matrixValues.data()));
      aaMatrixDiagonal.bindToLogicalRegion(*(A.pic.matrixDiagonal.data()));
    aaLocalToGlobalMap.bindToLogicalRegion(*(A.pic.localToGlobalMap.data()));
    aaGlobalToLocalMap.bindToLogicalRegion(*(A.pic.globalToLocalMap.data()));
    // For convenience let's refresh members for easier access in subsequent
    // uses. NOTE: these members are only valid during the life of a task.
    A.refreshMembers(
        SparseMatrix(
            A.geom,
            A.localData,
            nonzerosInRow,
            mtxIndG1D,
            mtxIndL,
            matrixValues1D,
            matrixDiagonal,
            localToGlobalMap,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        )
    );
}
