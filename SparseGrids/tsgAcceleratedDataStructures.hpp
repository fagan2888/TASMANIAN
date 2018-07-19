/*
 * Copyright (c) 2017, Miroslav Stoyanov
 *
 * This file is part of
 * Toolkit for Adaptive Stochastic Modeling And Non-Intrusive ApproximatioN: TASMANIAN
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * UT-BATTELLE, LLC AND THE UNITED STATES GOVERNMENT MAKE NO REPRESENTATIONS AND DISCLAIM ALL WARRANTIES, BOTH EXPRESSED AND IMPLIED.
 * THERE ARE NO EXPRESS OR IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, OR THAT THE USE OF THE SOFTWARE WILL NOT INFRINGE ANY PATENT,
 * COPYRIGHT, TRADEMARK, OR OTHER PROPRIETARY RIGHTS, OR THAT THE SOFTWARE WILL ACCOMPLISH THE INTENDED RESULTS OR THAT THE SOFTWARE OR ITS USE WILL NOT RESULT IN INJURY OR DAMAGE.
 * THE USER ASSUMES RESPONSIBILITY FOR ALL LIABILITIES, PENALTIES, FINES, CLAIMS, CAUSES OF ACTION, AND COSTS AND EXPENSES, CAUSED BY, RESULTING FROM OR ARISING OUT OF,
 * IN WHOLE OR IN PART THE USE, STORAGE OR DISPOSAL OF THE SOFTWARE.
 */

#ifndef __TASMANIAN_SPARSE_GRID_ACCELERATED_DATA_STRUCTURES_HPP
#define __TASMANIAN_SPARSE_GRID_ACCELERATED_DATA_STRUCTURES_HPP

#include "tsgEnumerates.hpp"

namespace TasGrid{

class BaseAccelerationData{
public:
    BaseAccelerationData();
    virtual ~BaseAccelerationData();

    virtual bool isCompatible(TypeAcceleration acc) const = 0;

    virtual void resetGPULoadedData() = 0;
};

// wrapper around cublas and cusparse handles, and magma queue
// it also stores the values and data needed for cuda evaluations
// (points, support, etc.)
class AccelerationDataGPUFull : public BaseAccelerationData{
public:
    AccelerationDataGPUFull();
    ~AccelerationDataGPUFull();

    void setLogStream(std::ostream *os);
    // for GPU error messages/codes

    bool isCompatible(TypeAcceleration acc) const;
    // when setting a new acceleration, check if we need to also set a new object or can reuse this one

    double* getGPUValues() const;
    void loadGPUValues(size_t total_entries, const double *cpu_values);
    // either values (global grid) or coefficients (sequence, local polynomial, etc.)

    double* getGPUNodes() const;
    double* getGPUSupport() const;
    void loadGPUNodesSupport(int total_entries, const double *cpu_nodes, const double *cpu_support);
    // nodes and support for local polynomial evaluations (support also encodes order for special cases)

    int* getGPUpntr() const;
    int* getGPUindx() const;
    int* getGPUroots() const;
    void loadGPUHierarchy(int num_points, int *pntr, int *indx, int num_roots, int *roots);
    // collection of trees to be used in CUDA evaluations of local polynomial grids

    void resetGPULoadedData();
    // deletes nodes, values, and hierarchy

    void cublasDGEMV(int num_outputs, int num_points, const double cpu_weights[], double *cpu_result);
    void cublasDGEMM(int num_outputs, int num_x, int num_points, const double gpu_weights[], double *gpu_result);
    // dense matrix-matrix or matrix-vector product using cublas, the matrix is the getGPUValues()

    void cusparseMatmul(bool cpu_pointers, int num_points, int num_outputs, int num_x, const int *spntr, const int *sindx, const double *svals, int num_nz, double *result);
    // sparse matrix times dense matrix, dense matrix is getGPUValues(), sparse matrix can be given on either cpu or gpu

    void cusparseMatvec(int num_points, int num_x, const int *spntr, const int *sindx, const double *svals, int num_nz, double *result);
    // sparse matrix times a dense vector, makes sense only if the matrix already sits on the gpu
    // e.g., assumes num_points == 1, the vectors is getGPUValues(), and the sparse matrix was computed on the GPU

    void cusparseMatveci(int num_outputs, int num_points, int num_nz, const int *sindx, const double *svals, double *result);
    // dense matrix times a sparse vector defined by sindx and svals
    // the dense matrix is getGPUValues()

    void magmaCudaDGEMM(int gpuID, int num_outputs, int num_x, int num_points, const double gpu_weights[], double *gpu_result);
    void magmaCudaDGEMV(int gpuID, int num_outputs, int num_points, const double cpu_weights[], double *cpu_result);
    // dense matrix-matrix or matrix-vector product using magma, the matrix is the getGPUValues()

protected:
    void makeCuBlasHandle();
    void makeCuSparseHandle();

    void initializeMagma(int gpuID);

private:
    double *gpu_values;
    double *gpu_nodes, *gpu_support; // support is half support and encodes the level of the function, i.e., constant, linear, or quadratic
    int *gpu_hpntr, *gpu_hindx, *gpu_roots;

    #ifdef Tasmanian_ENABLE_CUDA
    void *cublasHandle;
    void *cusparseHandle;
    #endif

    #ifdef Tasmanian_ENABLE_MAGMA
    bool magma_initialized; // call init once per object (must simplify later)
    void *magmaCudaStream;
    void *magmaCudaQueue;
    #endif

    std::ostream *logstream;
};

// namespace realized in tsgCudaKernels.cu, each function corresponds to a CUDA kernel for evaluations of basis matrix, domain transform, or fallback linear algebra
namespace TasCUDA{
    // convert transformed points to the canonical domain, all inputs live on the GPU
    void dtrans2can(int dims, int num_x, int pad_size, const double *gpu_trans_a, const double *gpu_trans_b, const double *gpu_x_transformed, double *gpu_x_canonical);

    // evaluate local polynomial rules
    void devalpwpoly(int order, TypeOneDRule rule, int dims, int num_x, int num_points, const double *gpu_x, const double *gpu_nodes, const double *gpu_support, double *gpu_y);
    void devalpwpoly_sparse(int order, TypeOneDRule rule, int dims, int num_x, int num_points, const double *gpu_x, const double *gpu_nodes, const double *gpu_support,
                            int *gpu_hpntr, int *gpu_hindx, int num_roots, int *gpu_roots, int* &gpu_spntr, int* &gpu_sindx, double* &gpu_svals, int &num_nzm, std::ostream *os);
    void devalpwpoly_sparse_dense(int order, TypeOneDRule rule, int dims, int num_x, int num_points, const double *gpu_x, const double *gpu_nodes, const double *gpu_support,
                                 int *gpu_hpntr, int *gpu_hindx, int num_roots, int *gpu_roots, double *gpu_dense);

    // evaluate sequence grids (not done yet)
    //void devalseq(int dims, int num_x, int num_points, int num_nodes, const double *gpu_x, const double *gpu_nodes, const double *gpu_coeff, const int *points, double *gpu_dense);

    // lazy cuda dgemm, nowhere near as powerful as cuBlas, but does not depend on cuBlas
    // gpu_a is M by K, gpu_b is K by N, gpu_c is M by N, all in column-major format
    // on exit gpu_c = gpu_a * gpu_b
    void cudaDgemm(int M, int N, int K, const double *gpu_a, const double *gpu_b, double *gpu_c);

    // lazy cuda sparse dgemm, less efficient (especially for large N), but more memory conservative then cusparse as there is no need for a transpose
    // C is M x N, B is K x N (K is max(gpu_sindx)), both are given in row-major format, num_nz/spntr/sindx/svals describe row compressed A which is M by K
    // on exit C = A * B
    void cudaSparseMatmul(int M, int N, int num_nz, const int* gpu_spntr, const int* gpu_sindx, const double* gpu_svals, const double *gpu_B, double *gpu_C);

    // dense matrix A (column major) times a sparse vector defiend by num_nz, indx, and vals
    // A is M by N, C is M by 1,
    // on exit C = A * (indx, vals)
    void cudaSparseVecDenseMat(int M, int N, int num_nz, const double *A, const int *indx, const double *vals, double *C);

    // converts a sparse matrix to a dense representation (all data sits on the gpu and is pre-allocated)
    void convert_sparse_to_dense(int num_rows, int num_columns, const int *gpu_pntr, const int *gpu_indx, const double *gpu_vals, double *gpu_destination);
}

// generic error checking function and types I/O
namespace AccelerationMeta{
    TypeAcceleration getIOAccelerationString(const char * name);
    const char* getIOAccelerationString(TypeAcceleration accel);
    int getIOAccelerationInt(TypeAcceleration accel);
    bool isAccTypeFullMemoryGPU(TypeAcceleration accel);
    bool isAccTypeGPU(TypeAcceleration accel);

    TypeAcceleration getAvailableFallback(TypeAcceleration accel);

    void cudaCheckError(void *cudaStatus, const char *info, std::ostream *os);
    void cublasCheckError(void *cublasStatus, const char *info, std::ostream *os);
    void cusparseCheckError(void *cusparseStatus, const char *info, std::ostream *os);
}

// stores domain transforms, to be used by the top class TasmanianSparseGrid
class AccelerationDomainTransform{
public:
    AccelerationDomainTransform(int num_dimensions, const double *transform_a, const double *transform_b, std::ostream *os);
    ~AccelerationDomainTransform();

    double* getCanonicalPoints(int num_dimensions, int num_x, const double *gpu_transformed_x);

private:
    // these actually store the rate and shift and not the hard upper/lower limits
    #ifdef Tasmanian_ENABLE_CUDA
    double *gpu_trans_a, *gpu_trans_b;
    int padded_size;

    std::ostream *logstream;
    #endif // Tasmanian_ENABLE_CUDA
};

}

#endif // __TASMANIAN_SPARSE_GRID_ACCELERATED_DATA_STRUCTURES_HPP
