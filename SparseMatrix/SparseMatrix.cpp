#define _CRT_SECURE_NO_WARNINGS
#include "SparseMatrix.h"
#include <vector>
#include <iostream>
#include <cmath>
#include <omp.h>
#include <assert.h>

using namespace std;

SparseMatrix::SparseMatrix( int num_rows, int num_cols )
{
    // create a zero-matrix
    data = new SparseMatrixData( num_rows, num_cols );
    rc = new RC();
}
SparseMatrix::SparseMatrix( int num_rows, int num_cols, const double non_zero_value[],
                            const int col_index[], const int row_pointer[], int N )
{
    data = new SparseMatrixData( num_rows, num_cols, non_zero_value, col_index, row_pointer, N );
    rc = new RC();
}

SparseMatrix::SparseMatrix( int num_rows, int num_cols,
                            const std::vector<double> non_zero_value,
                            const std::vector<int> col_index,
                            const std::vector<int> row_pointer )
{
    assert( non_zero_value.size()==col_index.size() && row_pointer.size()==(unsigned)num_rows+1 && "Data size is invalid. " );
    if( non_zero_value.size()==0 )
    {
        data = new SparseMatrixData( num_rows, num_cols );
    }
    else
    {
        data = new SparseMatrixData( num_rows, num_cols,
                                     (const double*) &non_zero_value[0],
                                     (const int*)    &col_index[0],
                                     (const int*)    &row_pointer[0],
                                     (int) non_zero_value.size() );
    }
    rc = new RC();
}

SparseMatrix::~SparseMatrix(void)
{
    if( rc->Release()==0 )
    {
        delete data;
        delete rc;
    }
}

const SparseMatrix SparseMatrix::clone(void) const
{
    // deep copy of the data
    if( this->isZero() )
    {
        return SparseMatrix( this->row(), this->col() );
    }
    else
    {
        SparseMatrix m(
            this->row(),
            this->col(),
            (const double*)(this->data->getRow()->nzval ),
            (const int*)   (this->data->getRow()->colind ),
            (const int*)   (this->data->getRow()->rowptr ),
            this->data->getRow()->nnz );
        return m;
    }
}

SparseMatrix::SparseMatrix( const SparseMatrix& matrix )
{
    this->data = matrix.data;
    this->rc   = matrix.rc;
    this->rc->AddRef();
}

const SparseMatrix& SparseMatrix::operator=( const SparseMatrix& matrix )
{
    this->~SparseMatrix();
    this->data = matrix.data;
    this->rc   = matrix.rc;
    this->rc->AddRef();
    return *this;
}

const SparseMatrix SparseMatrix::t() const
{
    SparseMatrix m = this->clone();
    m.data->transpose();
    return m;
}


const SparseMatrix& SparseMatrix::operator*=( const double& value )
{
    this->data->multiply( value );
    return (*this);
}

const SparseMatrix& SparseMatrix::operator/=( const double& value )
{
    this->data->multiply( 1.0/value );
    return (*this);
}

bool SparseMatrix::updateData( int num_rows, int num_cols,
                               const std::vector<double> non_zero_value,
                               const std::vector<int> col_index,
                               const std::vector<int> row_pointer )
{
    if( non_zero_value.size()==0 )
    {
        delete data;
        data = new SparseMatrixData( num_rows, num_cols );
        return true;
    }
    else
    {
        return this->updateData( num_rows, num_cols,
                                 (const double*) &non_zero_value[0],
                                 (const int*)    &col_index[0],
                                 (const int*)    &row_pointer[0],
                                 (int) non_zero_value.size() );
    }
}

bool SparseMatrix::updateData(  int num_rows, int num_cols,
                                const double non_zero_value[],
                                const int col_index[],
                                const int row_pointer[],
                                int N )
{
    if( rc->num()!=1 )
    {
        std::cout << "Unable to update data, there are more than one reference. " << std::endl;
        return false;
    }

    delete data;
    data = new SparseMatrixData( num_rows, num_cols, non_zero_value, col_index, row_pointer, N );
    return true;
}

bool SparseMatrix::updateData(  int num_rows, int num_cols,
                                double non_zero_value[],
                                int col_index[],
                                int row_pointer[],
                                int N )
{
    if( rc->num()!=1 )
    {
        std::cout << "Unable to update data, there are more than one reference. " << std::endl;
        return false;
    }

    delete data;
    data = new SparseMatrixData( num_rows, num_cols, non_zero_value, col_index, row_pointer, N );
    return true;
}



void SparseMatrix::getRowMatrixData( int& N, double const** non_zero_value, int const** column_index, int const** row_pointer ) const
{
    if( const SparseMatrixDataRow* rowData = this->data->getRow() )
    {
        N = rowData->nnz;
        *non_zero_value = rowData->nzval;
        *column_index = rowData->colind;
        *row_pointer = rowData->rowptr;
    }
    else
    {
        N = 0;
        *non_zero_value = 0;
        *column_index = 0;
        *row_pointer = 0;
    }
}














const SparseMatrix operator*( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.col()==m2.row() && "Matrix size does not match" );

    if( m1.isZero() || m2.isZero() )
    {
        // if either m1 or m2 is zero matrix, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }

    vector<double> res_nzval;
    vector<int> res_colidx;
    vector<int> res_rowptr;

    const double* const nzval1 = m1.data->getRow()->nzval;
    const int* const colidx1   = m1.data->getRow()->colind;
    const int* const rowptr1   = m1.data->getRow()->rowptr;

    const double* const nzval2 = m2.data->getCol()->nzval;
    const int* const rowidx2   = m2.data->getCol()->rowind;
    const int* const colptr2   = m2.data->getCol()->colptr;

    // store the result as row-order
    res_rowptr.push_back( 0 );
    for( int r=0; r < m1.row(); r++ )
    {
        for( int c=0; c < m2.col(); c++ )
        {
            int r1 = rowptr1[r];
            int c2 = colptr2[c];
            double v = 0.0;
            while( r1!=rowptr1[r+1] && c2!=colptr2[c+1] )
            {
                if( colidx1[r1]==rowidx2[c2] ) v += nzval1[r1++] * nzval2[c2++];
                else if( colidx1[r1]<rowidx2[c2] ) r1++;
                else c2++;
            }
            if( v!=0.0 )
            {
                res_nzval.push_back( v );
                res_colidx.push_back( c );
            }
        }
        res_rowptr.push_back( (int) res_nzval.size() );
    }

    // use (const T*) to force the constructor to make a deep copy of the data
    if( res_nzval.size()==0 )
    {
        return SparseMatrix( m1.row(), m2.col() );
    }
    else
    {
        SparseMatrix res( m1.row(), m2.col(),
                          (const double*) (&res_nzval[0]),
                          (const int*)    (&res_colidx[0]),
                          (const int*)    (&res_rowptr[0]),
                          (int) res_nzval.size() );
        return res;
    }
}

const SparseMatrix operator-( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.row()==m2.row() && m1.col()==m2.col() && "Matrix size does not match" );


    if( m1.data->getRow()==NULL && m2.data->getRow()==NULL )
    {
        // if both of the are zero, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }
    else if( m1.data->getRow()==NULL )
    {
        SparseMatrix res = m2.clone();
        return res*=-1.0;
    }
    else if( m2.data->getRow()==NULL )
    {
        return m1.clone();
    }

    vector<double> res_nzval;
    vector<int> res_colidx;
    vector<int> res_rowptr;

    const double* const nzval1 = m1.data->getRow()->nzval;
    const int* const colidx1   = m1.data->getRow()->colind;
    const int* const rowptr1   = m1.data->getRow()->rowptr;

    const double* const nzval2 = m2.data->getRow()->nzval;
    const int* const colidx2   = m2.data->getRow()->colind;
    const int* const rowptr2   = m2.data->getRow()->rowptr;

    // store the result as row-order
    res_rowptr.push_back( 0 );
    for( int r=0; r < m1.row(); r++ )
    {
        int i1 = rowptr1[r];
        int i2 = rowptr2[r];
        while( i1<rowptr1[r+1] && i2<rowptr2[r+1] )
        {
            const int& c1 = colidx1[i1];
            const int& c2 = colidx2[i2];

            double v = 0;
            int c = min(c1, c2);
            if( c1==c2 )
            {
                v = nzval1[i1++] - nzval2[i2++];
                if( std::abs(v) < 1e-35 ) continue;
            }
            else if( c1 < c2 )
            {
                v = nzval1[i1++];
            }
            else
            {
                v = -nzval2[i2++];
            }
            res_nzval.push_back( v );
            res_colidx.push_back( c );
        }
        while( i1<rowptr1[r+1] )
        {
            res_nzval.push_back( nzval1[i1] );
            res_colidx.push_back( colidx1[i1] );
            i1++;
        }
        while( i2<rowptr2[r+1] )
        {
            res_nzval.push_back( -nzval2[i2] );
            res_colidx.push_back( colidx2[i2] );
            i2++;
        }

        res_rowptr.push_back( (int) res_nzval.size() );
    }

    // use (const T*) to force the constructor to make a deep copy of the data
    SparseMatrix res( m1.row(), m2.col(), res_nzval, res_colidx, res_rowptr );

    return res;
}



const SparseMatrix operator+( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.row()==m2.row() && m1.col()==m2.col() && "Matrix size does not match" );

    if( m1.data->getRow()==NULL && m2.data->getRow()==NULL )
    {
        // if both of the are zero, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }
    else if( m1.data->getRow()==NULL )
    {
        return m2.clone();
    }
    else if( m2.data->getRow()==NULL )
    {
        return m1.clone();
    }

    vector<double> res_nzval;
    vector<int> res_colidx;
    vector<int> res_rowptr;

    const double* const nzval1 = m1.data->getRow()->nzval;
    const int* const colidx1   = m1.data->getRow()->colind;
    const int* const rowptr1   = m1.data->getRow()->rowptr;

    const double* const nzval2 = m2.data->getRow()->nzval;
    const int* const colidx2   = m2.data->getRow()->colind;
    const int* const rowptr2   = m2.data->getRow()->rowptr;

    // store the result as row-order
    res_rowptr.push_back( 0 );
    for( int r=0; r < m1.row(); r++ )
    {
        int i1 = rowptr1[r];
        int i2 = rowptr2[r];
        while( i1!=rowptr1[r+1] && i2!=rowptr2[r+1] )
        {
            if( colidx1[i1]==colidx2[i2] )
            {
                double v = nzval1[i1] + nzval2[i2];
                if( abs(v)>1e-35 )
                {
                    res_nzval.push_back( v );
                    res_colidx.push_back( colidx1[i1] );
                }
                i1++;
                i2++;
            }
            else if( colidx1[i1]<colidx2[i2] )
            {
                res_nzval.push_back( nzval1[i1] );
                res_colidx.push_back( colidx1[i1] );
                i1++;
            }
            else
            {
                res_nzval.push_back( nzval2[i2] );
                res_colidx.push_back( colidx2[i2] );
                i2++;
            }
        }
        while( i1<rowptr1[r+1] )
        {
            res_nzval.push_back( nzval1[i1] );
            res_colidx.push_back( colidx1[i1] );
            i1++;
        }
        while( i2<rowptr2[r+1] )
        {
            res_nzval.push_back( nzval2[i2] );
            res_colidx.push_back( colidx2[i2] );
            i2++;
        }
        res_rowptr.push_back( (int) res_nzval.size() );
    }

    // use (const T*) to force the constructor to make a deep copy of the data
    SparseMatrix res( m1.row(), m2.col(),
                      (const double*) (&res_nzval[0]),
                      (const int*)    (&res_colidx[0]),
                      (const int*)    (&res_rowptr[0]),
                      (int) res_nzval.size() );

    return res;
}


const SparseMatrix operator/( const SparseMatrix& m1, const double& value )
{
    SparseMatrix sm = m1.clone();
    return (sm /= value);
}


const SparseMatrix operator*( const SparseMatrix& m1, const double& value )
{
    SparseMatrix sm = m1.clone();
    return (sm *= value);
}

ostream& operator<<( ostream& out, const SparseMatrix& m )
{
    cout << "Size: " << m.row() << " x " << m.col() << endl;

    if( m.data->getRow()==NULL )
    {
        out << "  ...This is a zero matrix." << endl;
        return out;
    }

    const double* const nzval = m.data->getRow()->nzval;
    const int* const colidx   = m.data->getRow()->colind;
    const int* const rowptr   = m.data->getRow()->rowptr;

    int vi = 0;
    for( int r=0; r<m.row(); r++ )
    {
        cout << "   ";
        for( int c=0; c<m.col(); c++ )
        {
            out.width( 4 );
            if( colidx[vi]==c && vi<rowptr[r+1] ) out << nzval[vi++] << " ";
            else out << 0 << " ";
        }
        out << endl;
    }

    return out;
}

void SparseMatrix::print( ostream& out ) const
{
    out << "Matrix Size: " << this->row() << " x " << this->col() << endl;

    if( this->data->getRow()==NULL )
    {
        out << "  ...This is a zero matrix." << endl;
    }

    const int& N              = this->data->getRow()->nnz;
    const double* const nzval = this->data->getRow()->nzval;
    const int* const colidx   = this->data->getRow()->colind;
    const int* const rowptr   = this->data->getRow()->rowptr;

    cout << "# non-zero values: " << N << endl;

    int r = 0;
    for( int i = 0; i<N; i++ )
    {
        if( i >= rowptr[r+1] ) r++;
        out << "   [";
        out << std::left << r << ", ";
        out << colidx[i] << "]: ";
        out << nzval[i] << " ";
        out << endl;
    }
}

SparseMatrix SparseMatrix::diag() const
{
    assert( this->row()==this->col() && "Matrix size does not match" );

    const double* const nzval = this->data->getRow()->nzval;
    const int* const colidx   = this->data->getRow()->colind;
    const int* const rowptr   = this->data->getRow()->rowptr;

    // store the result as row-order
    vector<double> res_nzval;
    vector<int>    res_colidx;
    vector<int>    res_rowptr;

    res_rowptr.push_back( 0 );
    for( int r=0; r < this->row(); r++ )
    {
        for( int i = rowptr[r]; i < rowptr[r+1]; i++ )
        {
            const int& c = colidx[i];
            if( c >= r )
            {
                if( c==r )
                {
                    res_nzval.push_back( nzval[i] );
                    res_colidx.push_back( c );
                }
                break;
            }
        }
        res_rowptr.push_back( (int) res_nzval.size() );
    }

    // use (const T*) to force the constructor to make a deep copy of the data
    return SparseMatrix( this->row(), this->col(),
                         (const double*) (&res_nzval[0]),
                         (const int*)    (&res_colidx[0]),
                         (const int*)    (&res_rowptr[0]),
                         (int) res_nzval.size() );
}

const SparseMatrix multiply_openmp( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.col()==m2.row() && "Matrix size does not match" );

    if( m1.data->getRow()==NULL || m2.data->getCol()==NULL )
    {
        // if either m1 or m2 is zero matrix, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }


    const double* const nzval1 = m1.data->getRow()->nzval;
    const int* const colidx1   = m1.data->getRow()->colind;
    const int* const rowptr1   = m1.data->getRow()->rowptr;

    const double* const nzval2 = m2.data->getCol()->nzval;
    const int* const rowidx2   = m2.data->getCol()->rowind;
    const int* const colptr2   = m2.data->getCol()->colptr;

    vector<double> res_nzval;
    vector<int> res_colidx;
    vector<int> res_rowptr = vector<int>( m1.row()+1, 0 );

    // maximum number of thread
    int max_num_threads = omp_get_max_threads();
    // number of non-zero value obtained by each thread
    vector<unsigned int> thread_nzv_size( max_num_threads, 0);

    #pragma omp parallel
    {
        // local variables
        vector<double> res_nzval_loc;
        vector<int> res_colidx_loc;

        #pragma omp for

        for( int r=0; r < m1.row(); r++ )
        {
            int old_row_size = (int) res_nzval_loc.size();
            for( int c=0; c < m2.col(); c++ )
            {
                int r1 = rowptr1[r];
                int c2 = colptr2[c];
                double v = 0.0;
                while( r1!=rowptr1[r+1] && c2!=colptr2[c+1] )
                {
                    if( colidx1[r1]==rowidx2[c2] ) v += nzval1[r1++] * nzval2[c2++];
                    else if( colidx1[r1]<rowidx2[c2] ) r1++;
                    else c2++;
                }
                if( v!=0.0 )
                {
                    res_nzval_loc.push_back( v );
                    res_colidx_loc.push_back( c );
                }
            }
            res_rowptr[r+1] = (int) res_nzval_loc.size() - old_row_size;
        }

        // obtatin current thread id
        int tid = omp_get_thread_num();
        thread_nzv_size[tid] = (int) res_nzval_loc.size();

        #pragma omp barrier

        #pragma omp single
        {
            // accumulate the size of each column
            for( int r=0; r < m1.row(); r++ )
            {
                res_rowptr[r+1] += res_rowptr[r];
            }

            // accumulate the size of each thread
            int nThreads = omp_get_num_threads();
            for( int i=1; i<nThreads; i++ )
            {
                thread_nzv_size[i] += thread_nzv_size[i-1];
            }

            // resize the result vector, make them bigger!
            res_nzval = vector<double>( res_rowptr.back() );
            res_colidx = vector<int>( res_rowptr.back() );
        }

        #pragma omp critical
        {
            int nzv_offset = ( tid > 0 ) ? thread_nzv_size[tid-1] : 0;
            int data_size = thread_nzv_size[tid] - nzv_offset;
            memcpy( &res_nzval[nzv_offset], &res_nzval_loc[0], data_size*sizeof(double) );
            memcpy( &res_colidx[nzv_offset], &res_colidx_loc[0], data_size*sizeof(int) );
        }
    }

    // use (const T*) to force the constructor to make a deep copy of the data
    SparseMatrix res( m1.row(), m2.col(),
                      (const double*) (&res_nzval[0]),
                      (const int*)    (&res_colidx[0]),
                      (const int*)    (&res_rowptr[0]),
                      (int) res_nzval.size() );

    return res;
}
