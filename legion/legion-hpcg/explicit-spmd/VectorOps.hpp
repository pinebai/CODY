/**
 * Copyright (c)      2017 Los Alamos National Security, LLC
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
    @file VectorOps.hpp

    HPCG data structure operations for dense vectors.
 */

#pragma once

#include "Geometry.hpp"
#include "hpcg.hpp"
#include "LegionArrays.hpp"

#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <fstream>

/*!
    Fill the input vector with zero values.

    @param[inout] v - On entrance v is initialized, on exit all its values are
                      zero.
 */
inline void
ZeroVector(
    Array<floatType> &v,
    Context,
    Runtime *
) {
    const local_int_t localLength = v.length();
    floatType *const vv = v.data();
    for (local_int_t i = 0; i < localLength; ++i) vv[i] = 0.0;
}

/*!
    Copy input vector to output vector.

    @param[in] v Input vector.
    @param[in] w Output vector.
 */
inline void
CopyVector(
    Array<floatType> &v,
    Array<floatType> &w,
    Context,
    Runtime *
) {
    const local_int_t localLength = v.length();
    assert(w.length() >= size_t(localLength));

    const floatType *const vv = v.data();
    floatType *const wv = w.data();
    //
    for (local_int_t i = 0; i < localLength; ++i) wv[i] = vv[i];
}

/**
 *
 */
void
PrintVector(
    Array<floatType> &v,
    Context,
    Runtime *
) {
    using namespace std;

    const decltype(v.length()) w = 8;
    const decltype(v.length()) brk = 80;

    floatType *vd = v.data();

    for (decltype(v.length()) i = 0; i < v.length(); ++i) {
        if (0 == i % (brk / w)) cout << endl;
        cout << scientific << setprecision(1) << vd[i] << " ";
    }
}

/**
 *
 */
void
PrintVector(
    Array<floatType> &v,
    std::string fName,
    Context,
    Runtime *
) {
    using namespace std;

    FILE *file = fopen(fName.c_str(), "w+");
    assert(file);

    const floatType *const vd = v.data();

    for (decltype(v.length()) i = 0; i < v.length(); ++i) {
        fprintf(file, "%.1e\n", vd[i]);
    }

    fclose(file);
}

inline void
ColorVector(
    Array<floatType> &v,
    int color,
    Context,
    Runtime *
) {
    const local_int_t localLength = v.length();
    floatType *const vv = v.data();
    for (local_int_t i = 0; i < localLength; ++i) vv[i] = floatType(color);
}

/*!
    Fill the input vector with pseudo-random values.

    @param[in] v
 */
inline void
FillRandomVector(
    Array<floatType> &v,
    Context,
    Runtime *
) {
    const local_int_t localLength = v.length();
    floatType *const vv = v.data();
    for (int i = 0; i < localLength; ++i) {
        vv[i] = rand() / (floatType)(RAND_MAX) + 1.0;
    }
}

/*!
    Multiply (scale) a specific vector entry by a given value.

    @param[inout] v Vector to be modified.
    @param[in] index Local index of entry to scale.
    @param[in] value Value to scale by.
 */
inline void
ScaleVectorValue(
    Array<floatType> &v,
    local_int_t index,
    floatType value,
    Context,
    Runtime *
) {
    assert(index >= 0 && index < local_int_t(v.length()));
    //
    floatType *const vv = v.data();
    vv[index] *= value;
}