#ifndef __FFT_H__
#define __FFT_H__

#include <vector>

#ifndef audio_real
#define audio_real float
#endif

//#define __FFT_IPP__

#ifndef uint
typedef unsigned int uint;
#endif

struct Cmplx
{
	Cmplx() {};
	~Cmplx() {};
	audio_real re;
	audio_real im;
};

enum FftModes
{
	FFT_REAL,
	FFT_COMPLEX
};

class Fft
{
public:
	//! Empty default constructor
	Fft();
	//! Destructor
	~Fft();
	//! Sets FFT size and mode (real or complex)
	void SetMode(FftModes iMode, uint sFFT);
	//! Forward real FFT
	//  Input: X[N] real points
	//  Output: Sp[N/2+1] complex points (Sp should contain sFFT complex elements for working calculations)
	//  X and Sp can be either equal or non-overlapping buffers
	void FftReal(const audio_real *X, Cmplx *Sp) const;
	//! Inverse real FFT
	//  Sp should contain sFFT complex elements for working calculations
	//  Input: Sp[N/2+1] complex points (contents of Sp is destroyed)
	//  Output: X[N] real points, Sp and X cannot overlap
	void InvFftReal(Cmplx *Sp, audio_real *X) const;
	//! Forward complex FFT
	//  Input: X[N] complex points
	//  Output: X[N] complex spectrum points
	void FftComplex_InPlace(Cmplx *X) const;
	//! Inverse complex FFT
	//  Input: X[N] complex spectrum points
	//  Output: X[N] complex points
	void InvFftComplex_InPlace(Cmplx *X) const;

	//! Optimized versions for particular FFT sizes
	void FftReal16(const audio_real *X, Cmplx *Sp) const;
	void FftComplex8_InPlace(Cmplx *X) const;

protected:
	uint m_sFFT;	//!< FFT size
	#ifdef __FFT_IPP__
		IPP::FFTState_Float *m_Spec;	//!< FFT state structure
		unsigned char *m_Buf;	//!< FFT working buffer
	#else
		uint m_uOrder;	//!< FFT size order
		std::vector<uint> m_Perm;	//!< Table of permutation indexes for bit reversing algorithm
		std::vector<audio_real> m_Rotors;	//!< Table of rotating multipliers
		Fft *m_pChild;	//!< Pointer to complex FFT class used for calculation of real FFT
	#endif
};

#endif // multiple inclusion