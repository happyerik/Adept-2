/* Packet.h -- Vectorization support

    Copyright (C) 2016-2017 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

   A Packet contains a short vector of values, and when it is used in
   a limited set of arithmetic operations, the appropriate vector
   instructions will be used.  For example if your hardware and
   compiler support SSE2 then Packet<float> is a vector of 4x 4-byte
   floats while Packet<double> is a vector of 2x 8-byte floats. This
   header file also provides for allocating aligned data
*/

#ifndef AdeptPacket_H
#define AdeptPacket_H 1

#include <iostream>
#include <cstdlib>

// Headers needed for x86 vector intrinsics
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __AVX__
#include <immintrin.h>
#endif

// Headers needed for allocation of aligned memory
#include <new>

#ifdef __unix__
#include <unistd.h>  // Defines _POSIX_VERSION
#endif
#include <stdlib.h>
#ifdef _MSC_VER
#include <malloc.h> // Provides _aligned_malloc on Windows
#endif

#include "base.h"

// -------------------------------------------------------------------
// Determine how many floating point values will be held in a packet
// -------------------------------------------------------------------

#ifndef ADEPT_FLOAT_PACKET_SIZE
#  ifdef __AVX__
#    define ADEPT_FLOAT_PACKET_SIZE 8
#  elif defined(__SSE2__)
#    define ADEPT_FLOAT_PACKET_SIZE 4
#  else
#    define ADEPT_FLOAT_PACKET_SIZE 1
#  endif
#endif
#ifndef ADEPT_DOUBLE_PACKET_SIZE
#  ifdef __AVX__
#    define ADEPT_DOUBLE_PACKET_SIZE 4
#  elif defined(__SSE2__)
#    define ADEPT_DOUBLE_PACKET_SIZE 2
#  else
#    define ADEPT_DOUBLE_PACKET_SIZE 1
#  endif
#endif

// -------------------------------------------------------------------
// Determine how many floating point values will be held in packet of Real
// -------------------------------------------------------------------

#if ADEPT_REAL_TYPE_SIZE == 4
#define ADEPT_REAL_PACKET_SIZE ADEPT_FLOAT_PACKET_SIZE
#elif ADEPT_REAL_TYPE_SIZE == 8
#define ADEPT_REAL_PACKET_SIZE ADEPT_DOUBLE_PACKET_SIZE
#else
#define ADEPT_REAL_PACKET_SIZE 1
#endif

namespace adept {

  namespace internal {

    // -------------------------------------------------------------------
    // Default packet types containing only one value
    // -------------------------------------------------------------------

    template <typename T>
    struct Packet {
      typedef T intrinsic_type;
      static const int size = 1;
      static const std::size_t alignment_bytes = sizeof(T);
      static const bool is_vectorized = false;
      Packet() : data(0.0) { }
      explicit Packet(const T* d) : data(*d) { }
      explicit Packet(T d) : data(d) { }
      void put(T* __restrict d) const { *d = data; }
      void operator=(T d)  { data=d; }
      void operator+=(T d) { data+=d; }
      void operator-=(T d) { data-=d; }
      void operator*=(T d) { data*=d; }
      void operator/=(T d) { data/=d; }
      union {
	T data;
	T value;
      };
    };

    // -------------------------------------------------------------------
    // Define a specialization, and the basic mathematical operators
    // supported in hardware, for Packet in the case that each
    // contains a single SSE2/AVX intrinsic data object.
    // -------------------------------------------------------------------

#define ADEPT_DEF_PACKET_TYPE(TYPE, INT_TYPE, SET0,	        \
			      LOAD, LOADU, SET1, STORE, STOREU,	\
			      ADD, SUB, MUL, DIV, SQRT)		\
    template <> struct Packet<TYPE> {				\
      typedef INT_TYPE intrinsic_type;				\
      static const int size = sizeof(INT_TYPE) / sizeof(TYPE);	\
      static const int intrinsic_size				\
        = sizeof(INT_TYPE) / sizeof(TYPE);			\
      static const std::size_t alignment_bytes = sizeof(INT_TYPE);	\
      static const bool is_vectorized = true;			\
      Packet()              : data(SET0())  { }			\
      Packet(const TYPE* d) : data(LOAD(d)) { }		\
      Packet(const TYPE* d, int) : data(LOADU(d)) { }		\
      Packet(TYPE d)        : data(SET1(d)) { }			\
      Packet(INT_TYPE d)    : data(d) { }			\
      void put(TYPE* __restrict d) const { STORE(d, data); }	\
      void put_unaligned(TYPE* __restrict d) const		\
      { STOREU(d, data); }					\
      void operator=(INT_TYPE d) { data=d; }			\
      void operator=(const Packet<TYPE>& __restrict d)		\
      { data=d.data; }						\
      void operator+=(const Packet<TYPE>& __restrict d)		\
      { data = ADD(data, d.data); }				\
      void operator-=(const Packet<TYPE>& __restrict d)		\
      { data = SUB(data, d.data); }				\
      void operator*=(const Packet<TYPE>& __restrict d)		\
      { data = MUL(data, d.data); }				\
      void operator/=(const Packet<TYPE>& __restrict d)		\
      { data = DIV(data, d.data); }				\
      union {							\
	INT_TYPE data;						\
	TYPE value;						\
      };							\
    };								\
    inline							\
    std::ostream& operator<<(std::ostream& os,			\
			     const Packet<TYPE>& x) {		\
      TYPE d[Packet<TYPE>::size];				\
      STOREU(d, x.data);						\
      os << "(";						\
      for (int i = 0; i < Packet<TYPE>::size; ++i) {		\
	os << " " << d[i];					\
      }								\
      os << ")";						\
      return os;						\
    }								\
    inline							\
    Packet<TYPE> operator+(const Packet<TYPE>& __restrict x,	\
			   const Packet<TYPE>& __restrict y)	\
    { return ADD(x.data,y.data); }				\
    inline							\
    Packet<TYPE> operator-(const Packet<TYPE>& __restrict x,	\
			   const Packet<TYPE>& __restrict y)	\
    { return SUB(x.data,y.data); }				\
    inline							\
    Packet<TYPE> operator*(const Packet<TYPE>& __restrict x,	\
			   const Packet<TYPE>& __restrict y)	\
    { return MUL(x.data,y.data); }				\
    inline							\
    Packet<TYPE> operator/(const Packet<TYPE>& __restrict x,	\
			   const Packet<TYPE>& __restrict y)	\
    { return DIV(x.data,y.data); }				\

  /*
  inline						\
  INT_TYPE sqrt(const Packet<TYPE>& x)			\
  { return SQRT(x.data); }
  */

    //#ifdef ADEPT_BLEEDING_EDGE

    // -------------------------------------------------------------------
    // Define single-precision Packet
    // -------------------------------------------------------------------
#ifdef __AVX__

#if ADEPT_FLOAT_PACKET_SIZE == 4
    // Need to use SSE2
    ADEPT_DEF_PACKET_TYPE(float, __m128, _mm_setzero_ps, 
			  _mm_load_ps, _mm_loadu_ps, _mm_set1_ps,
			  _mm_store_ps, _mm_storeu_ps,
			  _mm_add_ps, _mm_sub_ps,
			  _mm_mul_ps, _mm_div_ps, _mm_sqrt_ps); 
#elif ADEPT_FLOAT_PACKET_SIZE == 8
    // Use AVX
    ADEPT_DEF_PACKET_TYPE(float, __m256, _mm256_setzero_ps,
			  _mm256_load_ps, _mm256_loadu_ps, _mm256_set1_ps,
			  _mm256_store_ps, _mm256_storeu_ps,
			  _mm256_add_ps, _mm256_sub_ps,
			  _mm256_mul_ps, _mm256_div_ps, _mm256_sqrt_ps);
#elif ADEPT_FLOAT_PACKET_SIZE != 1
#error With AVX, ADEPT_FLOAT_PACKET_SIZE must be 1, 4 or 8
#endif

#elif __SSE2__

#if ADEPT_FLOAT_PACKET_SIZE == 4
    ADEPT_DEF_PACKET_TYPE(float, __m128, _mm_setzero_ps, 
			  _mm_load_ps, _mm_loadu_ps, _mm_set1_ps,
			  _mm_store_ps, _mm_storeu_ps,
			  _mm_add_ps, _mm_sub_ps,
			  _mm_mul_ps, _mm_div_ps, _mm_sqrt_ps); 
#elif ADEPT_FLOAT_PACKET_SIZE != 1
#error With SSE2, ADEPT_FLOAT_PACKET_SIZE must be 1 or 4
#endif

#elif ADEPT_FLOAT_PACKET_SIZE > 1

#error ADEPT_FLOAT_PACKET_SIZE > 1 requires SSE2 or AVX

#endif

    // -------------------------------------------------------------------
    // Define double-precision Packet
    // -------------------------------------------------------------------
#ifdef __AVX__

#if ADEPT_DOUBLE_PACKET_SIZE == 2
    // Need to use SSE2
    ADEPT_DEF_PACKET_TYPE(double, __m128d, _mm_setzero_pd, 
			  _mm_load_pd, _mm_loadu_pd, _mm_set1_pd,
			  _mm_store_pd, _mm_storeu_pd,
			  _mm_add_pd, _mm_sub_pd,
			  _mm_mul_pd, _mm_div_pd, _mm_sqrt_pd);
#elif ADEPT_DOUBLE_PACKET_SIZE == 4
    ADEPT_DEF_PACKET_TYPE(double, __m256d, _mm256_setzero_pd, 
			  _mm256_load_pd, _mm256_loadu_pd, _mm256_set1_pd,
			  _mm256_store_pd, _mm256_storeu_pd,
			  _mm256_add_pd, _mm256_sub_pd,
			  _mm256_mul_pd, _mm256_div_pd, _mm256_sqrt_pd);
#elif ADEPT_DOUBLE_PACKET_SIZE != 1
#error With AVX, ADEPT_DOUBLE_PACKET_SIZE must be 1, 2 or 4
#endif

#elif defined(__SSE2__)

#if ADEPT_DOUBLE_PACKET_SIZE == 2
    ADEPT_DEF_PACKET_TYPE(double, __m128d, _mm_setzero_pd, 
			  _mm_load_pd, _mm_loadu_pd, _mm_set1_pd,
			  _mm_store_pd, _mm_storeu_pd,
			  _mm_add_pd, _mm_sub_pd,
			  _mm_mul_pd, _mm_div_pd, _mm_sqrt_pd);
#elif ADEPT_DOUBLE_PACKET_SIZE != 1
#error With SSE2, ADEPT_DOUBLE_PACKET_SIZE must be 1 or 2
#endif

#elif ADEPT_DOUBLE_PACKET_SIZE > 1

#error ADEPT_DOUBLE_PACKET_SIZE > 1 requires SSE2 or AVX

#endif
    
#undef ADEPT_DEF_PACKET_TYPE


    // -------------------------------------------------------------------
    // Aligned allocation and freeing of memory
    // -------------------------------------------------------------------
    template <typename Type>
    inline
    Type* alloc_aligned(Index n) {
      std::size_t n_align = Packet<Type>::alignment_bytes;
      if (n_align < sizeof(void*)) {
	// Note that the requested byte alignment passed to
	// posix_memalign must be at least sizeof(void*)
	return new Type[n];
      }
      else {
	Type* result;
#ifdef _POSIX_VERSION
#if _POSIX_VERSION >= 200112L
	if (posix_memalign(reinterpret_cast<void**>(&result), 
			   n_align, n*sizeof(Type)) != 0) {
	  throw std::bad_alloc();
	}
#else
	result = new Type[n];
#endif
#elif defined(_MSC_VER)
	result = reinterpret_cast<Type*>(_aligned_malloc(n*sizeof(Type),
							 n_align));
	if (result == 0) {
	  throw std::bad_alloc();
	}
#else
	result = new Type[n];	
#endif
      return result;
      }
    }
    
    template <typename Type>
    inline
    void free_aligned(Type* data) {
      if (Packet<Type>::alignment_bytes <= 1) {
	delete[] data;
      }
      else { 
#ifdef _POSIX_VERSION
#if _POSIX_VERSION >= 200112L   
	free(data);
#else
	delete[] data;
#endif
#elif defined(_MSC_VER)
	_aligned_free(ptr);
#endif
      }
    }

  } // End namespace internal

} // End namespace adept

#endif
