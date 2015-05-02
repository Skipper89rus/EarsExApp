#include "Fft.h"
#include <assert.h>
#include <math.h>

#ifndef ZASSERT
#define ZASSERT assert
#endif


#ifdef __FFT_IPP__

//////////////////////////////////////////////////////////////////////////////
/*! Empty default constructor */
Fft::Fft() :
	m_sFFT(0),
	m_Spec(0)
{
}

//////////////////////////////////////////////////////////////////////////////
/*! Destructor */
Fft::~Fft()
{
	if (m_Spec!=0)
	{
		Free(m_Buf);
		FFTFree(m_Spec);
	}
}

//////////////////////////////////////////////////////////////////////////////
/*! Sets FFT size */
void Fft::SetMode(uint FftSize)
{
	if (m_Spec!=0)
	{
		Free(m_Buf);
		FFTFree(m_Spec);
	}
	m_sFFT = FftSize;
	// Setup IPP structures
	int Order = 1;
	while ((1<<Order)<m_sFFT) ++Order;
	ZASSERT((1<<Order)==m_sFFT);	// FFT size must be a power of 2
	FFTInit(&m_Spec, Order);
	uint sBuffer;
	FFTGetBufSize(m_Spec, &sBuffer);
	m_Buf = Malloc_Byte(sBuffer);
}

//////////////////////////////////////////////////////////////////////////////
/*! Forward real FFT */
void Fft::FftRealCcs(audio_real *X, audio_real *Sp)
{
	FFTFwd(X, Sp, m_Spec, m_Buf);
}

//////////////////////////////////////////////////////////////////////////////
/*! Inverse real FFT */
void Fft::InvFftRealCcs(audio_real *Sp, audio_real *X)
{
	FFTInv(Sp, X, m_Spec, m_Buf);
}

#else	// __FFT_IPP__


namespace
{

	const audio_real fPi = audio_real(3.14159265358979323846);
	const double dPi = 3.14159265358979323846;

	//! Precalculates permutation indexes for bit-reverse algorithm
	void CachePerms(uint *pPerms, uint sFFT)
	{
		int i = 0;
		int k;
		for (int s=0; s<static_cast<signed>(sFFT)-1; ++s) {
			// Swap X[s] and X[i]
			*(pPerms++) = ( s < i ) ? i : s;
			k = sFFT >> 1;
			while ( i & k ) k >>= 1;
			i += k;
			k <<= 1;
			while (k < static_cast<signed>(sFFT)) {
				i -= k;
				k <<= 1;
			}
		} // end for s
	} // end CachePerms()

	//! Precalculates rotating multipliers for complex FFT
	void CacheRotors(audio_real *pRot, uint uOrder)
	{
		uint L = uOrder;
		uint LE = 1 << L;
		uint LE2 = LE >> 1;
		double UR = 1, UI = 0;
		double SR = cos(dPi/LE2), SI = -sin(dPi/LE2);
		for (uint j=0; j<LE2; ++j) {
			*(pRot++) = static_cast<audio_real>(UR);
			*(pRot++) = static_cast<audio_real>(UI);
			double TR = UR;
			UR = TR*SR - UI*SI;
			UI = TR*SI + UI*SR;
		} // end for rotor
	} // end CacheRotors()

} // end file scope


//////////////////////////////////////////////////////////////////////////////
/*! Empty default constructor */
Fft::Fft() :
	m_pChild(0)
{
}

//////////////////////////////////////////////////////////////////////////////
/*! Destructor */
Fft::~Fft()
{
	if ( m_pChild != 0 ) delete m_pChild;
}

//////////////////////////////////////////////////////////////////////////////
/*! Sets FFT size */
void Fft::SetMode(FftModes iMode, uint FftSize)
{
	ZASSERT(iMode==FFT_COMPLEX && FftSize>=1 || iMode==FFT_REAL && FftSize>=1);
	m_sFFT = FftSize;
	// Get FFT size order
	m_uOrder = 1;
	while ((1U<<m_uOrder)<m_sFFT) ++m_uOrder;
	ZASSERT((1U<<m_uOrder)==m_sFFT);	// FFT size must be a power of 2
	if ( iMode == FFT_COMPLEX ) {
		// Precalculate permutation indexes for bit-reverse algorithm
		m_Perm.resize( m_sFFT-1 );
		CachePerms( &m_Perm[0], m_sFFT );
		// Precalculate rotating multipliers for complex FFT
		m_Rotors.resize( m_sFFT );
		CacheRotors( &m_Rotors[0], m_uOrder );
	} // end if FFT_COMPLEX
	else {
		// Setup child complex FFT with m_sFFT/2 points
		if ( m_pChild != 0 ) delete m_pChild;
		m_pChild = new Fft;
		m_pChild->SetMode( FFT_COMPLEX, m_sFFT/2 );
		// Precalculate rotating multipliers for real FFT
		m_Rotors.resize( m_sFFT-2 );
		double UR = cos(2*dPi/m_sFFT);
		double UI = -sin(2*dPi/m_sFFT);
		double SR = UR, SI = UI;
		audio_real *pRot = &m_Rotors[0];
		for (uint i=1; i<m_sFFT/2; ++i) {
			*(pRot++) = static_cast<audio_real>(UR);
			*(pRot++) = static_cast<audio_real>(UI);
			double TR = UR;
			UR = TR*SR - UI*SI;
			UI = TR*SI + UI*SR;
		}
	} // end FFT_REAL
}

//////////////////////////////////////////////////////////////////////////////
/*! Forward real FFT */
void Fft::FftReal(const audio_real *X, Cmplx *Y) const
{
	if (m_sFFT == 1) {
		Y[0].re = X[0];
		Y[0].im = 0;
		return;
	}
	else if (m_sFFT == 2) {
		Y[0].re = X[0] + X[1];
		Y[1].re = X[0] - X[1];
		Y[0].im = Y[1].im = 0;
		return;
	}
	// Separate even and odd points
	const uint ND2 = m_sFFT>>1;
	const uint ND4 = ND2>>1;
	if ( static_cast<const void*>(X) != Y ) {
		const Cmplx *Z = reinterpret_cast<const Cmplx*>(X);
		for (uint i=0; i<ND2; ++i)
			Y[i] = Z[i];
	}
	// Calculate N/2 point complex FFT
	m_pChild->FftComplex_InPlace( Y );
	// Even/odd frequency domain decomposition
	for (uint i=1; i<ND4; ++i) {
		uint im = ND2 - i;
		uint ip2 = i + ND2;
		uint ipm = im + ND2;
		ZASSERT(im>=0 && im<m_sFFT && ipm>=0 && ipm<m_sFFT && ip2>=0 && ip2<m_sFFT);
		Y[ipm].re = Y[ip2].re = (Y[i].im + Y[im].im)*audio_real(0.5);
		Y[ip2].im = (Y[i].re - Y[im].re)*audio_real(-0.5);
		Y[ipm].im =  -Y[ip2].im;
		Y[im].re = Y[i].re = (Y[i].re + Y[im].re)*audio_real(0.5);
		Y[i].im = (Y[i].im - Y[im].im)*audio_real(0.5);
		Y[im].im = -Y[i].im;
	}
	Y[ND2+ND4].re = Y[ND4].im;
	Y[ND2].re = Y[0].im;
	Y[ND2+ND4].im = Y[ND2].im = Y[ND4].im = Y[0].im = 0;
	// Complete the last FFT stage
	// Calculate Y[0] and Y[N/2]
	audio_real TR = Y[ND2].re;
	audio_real TI = Y[ND2].im;
	Y[ND2].re = Y[0].re - TR;
	Y[ND2].im = Y[0].im - TI;
	Y[0].re += TR;
	Y[0].im += TI;
	// Calculate the rest of coefficients
	const audio_real *pRot = &m_Rotors[0];
	for (uint i=1; i<ND2; ++i) {
		Y[i].re += Y[i+ND2].re*pRot[0] - Y[i+ND2].im*pRot[1];
		Y[i].im += Y[i+ND2].re*pRot[1] + Y[i+ND2].im*pRot[0];
		pRot += 2;
	}
}

//////////////////////////////////////////////////////////////////////////////
/*! Inverse real FFT */
void Fft::InvFftReal(Cmplx *Y, audio_real *X) const
{
	const audio_real DivFFTSize = audio_real(1.0) / m_sFFT;
	if (m_sFFT == 1) {
		X[0] = Y[0].re;
		return;
	}
	else if (m_sFFT == 2) {
		X[0] = (Y[0].re + Y[1].re) * DivFFTSize;
		X[1] = (Y[0].re - Y[1].re) * DivFFTSize;
		return;
	}
	// Create the mirror image of the spectrum
	for (uint i=0; i<=(m_sFFT>>1); ++i)
		X[i] = Y[i].re + Y[i].im;
	for (uint i=(m_sFFT>>1)+1; i<m_sFFT; ++i)
		X[i] = Y[m_sFFT-i].re - Y[m_sFFT-i].im;
	// Calculate N-point real FFT
	FftReal( X, Y );
	// Adjust the resulting data
	for (uint i=0; i<=(m_sFFT>>1); ++i)
		X[i] = (Y[i].re + Y[i].im) * DivFFTSize;
	for (uint i=(m_sFFT>>1)+1; i<m_sFFT; ++i)
		X[i] = (Y[m_sFFT-i].re - Y[m_sFFT-i].im) * DivFFTSize;
}

//////////////////////////////////////////////////////////////////////////////
/*! Forward complex FFT */
void Fft::FftComplex_InPlace(Cmplx *X) const
{
	if (m_sFFT == 1) return;
	// Bit-reverse
	for (uint i=0; i<m_sFFT-1; ++i) {
		if ( m_Perm[i] != i ) {
			Cmplx t = X[i];
			X[i] = X[m_Perm[i]];
			X[m_Perm[i]] = t;
		}
	}
	// First pass
	for (uint i=0; i<m_sFFT; i+=2) {
		Cmplx t = X[i];
		X[i].re = t.re + X[i+1].re;
		X[i].im = t.im + X[i+1].im;
		X[i+1].re = t.re - X[i+1].re;
		X[i+1].im = t.im - X[i+1].im;
	}
	// Second pass
	if (m_sFFT > 2) {
		for (uint i=0; i<m_sFFT; i+=4) {
			Cmplx t = X[i];
			X[i].re = t.re + X[i+2].re;
			X[i].im = t.im + X[i+2].im;
			X[i+2].re = t.re - X[i+2].re;
			X[i+2].im = t.im - X[i+2].im;
			t = X[i+1];
			Cmplx z = X[i+3];
			X[i+1].re = t.re + z.im;
			X[i+1].im = t.im - z.re;
			X[i+3].re = t.re - z.im;
			X[i+3].im = t.im + z.re;
		}
	} // end if m_sFFT > 2
	// Last passes
	const audio_real *pRotors = &m_Rotors[0];
	for (uint L=3; L<=m_uOrder; ++L) {
		uint LE = 1 << L;
		uint LE2 = LE >> 1;
		uint uRotStep = 2 << (m_uOrder-L);
		uint uRotOfs = 0;
		for (uint j=0; j<LE2; ++j) {
			for (uint i=j; i<m_sFFT; i+=LE) {
				uint ip = i + LE2;
				audio_real TR = X[ip].re*pRotors[uRotOfs] - X[ip].im*pRotors[uRotOfs+1];
				audio_real TI = X[ip].re*pRotors[uRotOfs+1] + X[ip].im*pRotors[uRotOfs];
				X[ip].re = X[i].re - TR;
				X[ip].im = X[i].im - TI;
				X[i].re = X[i].re + TR;
				X[i].im = X[i].im + TI;
			} // end for butterfly
			uRotOfs += uRotStep;
		} // end for j
	} // end for level
}

//////////////////////////////////////////////////////////////////////////////
/*! Inverse complex FFT */
void Fft::InvFftComplex_InPlace(Cmplx *X) const
{
	if (m_sFFT == 1) return;
	audio_real DivFFTSize = audio_real(1.0) / m_sFFT;
	audio_real NDivFFTSize = -DivFFTSize;
	// Change the sign of ImX
	for (uint i=0; i<m_sFFT; ++i) X[i].im = -X[i].im;
	// Calculate N point complex FFT
	FftComplex_InPlace(X);
	// Adjust the resulting data
	for (uint i=0; i<m_sFFT; ++i) {
		X[i].re *= DivFFTSize;
		X[i].im *= NDivFFTSize;
	}
}

#endif


void Fft::FftReal16(const audio_real *X, Cmplx *Y) const
{
	// Separate even and odd points
	if ( static_cast<const void*>(X) != Y ) {
		const Cmplx *Z = reinterpret_cast<const Cmplx*>(X);
		for (uint i=0; i<8; ++i)
			Y[i] = Z[i];
	}
	// Calculate N/2 point complex FFT
	m_pChild->FftComplex8_InPlace( Y );
	// Even/odd frequency domain decomposition
	for (uint i=1; i<4; ++i) {
		uint im = 8 - i;
		uint ip2 = i + 8;
		uint ipm = im + 8;
		ZASSERT(im>=0 && im<m_sFFT && ipm>=0 && ipm<m_sFFT && ip2>=0 && ip2<m_sFFT);
		Y[ipm].re = Y[ip2].re = (Y[i].im + Y[im].im)*audio_real(0.5);
		Y[ip2].im = (Y[i].re - Y[im].re)*audio_real(-0.5);
		Y[ipm].im =  -Y[ip2].im;
		Y[im].re = Y[i].re = (Y[i].re + Y[im].re)*audio_real(0.5);
		Y[i].im = (Y[i].im - Y[im].im)*audio_real(0.5);
		Y[im].im = -Y[i].im;
	}
	Y[12].re = Y[4].im;
	Y[8].re = Y[0].im;
	Y[12].im = Y[8].im = Y[4].im = Y[0].im = 0;
	// Complete the last FFT stage
	// Calculate Y[0] and Y[N/2]
	audio_real TR = Y[8].re;
	audio_real TI = Y[8].im;
	Y[8].re = Y[0].re - TR;
	Y[8].im = Y[0].im - TI;
	Y[0].re += TR;
	Y[0].im += TI;
	// Calculate the rest of coefficients
	const audio_real *pRot = &m_Rotors[0];
	for (uint i=1; i<8; ++i) {
		Y[i].re += Y[i+8].re*pRot[0] - Y[i+8].im*pRot[1];
		Y[i].im += Y[i+8].re*pRot[1] + Y[i+8].im*pRot[0];
		pRot += 2;
	}
}


void Fft::FftComplex8_InPlace(Cmplx *X) const
{
	// Bit-reverse
	for (uint i=0; i<7; ++i) {
		if ( m_Perm[i] != i ) {
			Cmplx t = X[i];
			X[i] = X[m_Perm[i]];
			X[m_Perm[i]] = t;
		}
	}
	// First pass
	for (uint i=0; i<8; i+=2) {
		Cmplx t = X[i];
		X[i].re = t.re + X[i+1].re;
		X[i].im = t.im + X[i+1].im;
		X[i+1].re = t.re - X[i+1].re;
		X[i+1].im = t.im - X[i+1].im;
	}

	// Second pass
	Cmplx t = X[0];
	X[0].re += X[2].re;
	X[0].im += X[2].im;
	X[2].re = t.re - X[2].re;
	X[2].im = t.im - X[2].im;
	t = X[1];
	Cmplx z = X[3];
	X[1].re += z.im;
	X[1].im -= z.re;
	X[3].re = t.re - z.im;
	X[3].im = t.im + z.re;
	t = X[4];
	X[4].re += X[6].re;
	X[4].im += X[6].im;
	X[6].re = t.re - X[6].re;
	X[6].im = t.im - X[6].im;
	t = X[5];
	z = X[7];
	X[5].re += z.im;
	X[5].im -= z.re;
	X[7].re = t.re - z.im;
	X[7].im = t.im + z.re;

	// Last pass
	const audio_real *pRotors = &m_Rotors[0];
	uint uRotOfs = 0;
	for (uint j=0; j<4; ++j) {
		uint ip = j + 4;
		audio_real TR = X[ip].re*pRotors[uRotOfs] - X[ip].im*pRotors[uRotOfs+1];
		audio_real TI = X[ip].re*pRotors[uRotOfs+1] + X[ip].im*pRotors[uRotOfs];
		X[ip].re = X[j].re - TR;
		X[ip].im = X[j].im - TI;
		X[j].re += TR;
		X[j].im += TI;
		uRotOfs += 2;
	} // end for j
}