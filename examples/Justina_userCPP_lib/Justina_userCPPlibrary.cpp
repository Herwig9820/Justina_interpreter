/*************************************************************************************************************************
*   Example code demonstrating how to write a user c++ function library for use by the Justina interpreter               *
*                                                                                                                        *
*   The Justina interpreter library is licensed under the terms of the GNU General Public License v3.0 as published      *
*   by the Free Software Foundation (https://www.gnu.org/licenses).                                                      *
*   Refer to GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter            *
*                                                                                                                        *
*   This example code is in the public domain                                                                            *
*                                                                                                                        *
*   2024, Herwig Taveirne                                                                                                *
*************************************************************************************************************************/

#include "Justina_userCPPlibrary.h"

// -------------------------------------------------------------
// Example of a c++ 'function library' for direct use by Justina
// MORE INFORMATION: see USER MANUAL, available on GitHub
// --------------------------------------------------------------


// --------------------------------------------------------
// *   add two complex numbers in Cartesian coordinates   *
// -------------------------------------------------------
bool JustinaComplex::cmplxAdd(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered with identical Justina name):
    ---------------------------------------------------------------------
    var a(2), b(2), sum(2);             // arrays for 3 complex numbers
    ...                                 // input : a(1), b(1) = real part, a(2), b(2) = imaginary part
    cmplxAdd(a(1), b(1), sum(1));       // return: sum(1) = real part, sum(2) = imaginary (always pass an array element, not an array name)

*/

    // test arguments
    if (((valueType[0] & Justina::value_typeMask) != Justina::value_isFloat) ||     // floating point arrays expected
        ((valueType[1] & Justina::value_typeMask) != Justina::value_isFloat) ||
        ((valueType[2] & Justina::value_typeMask) != Justina::value_isFloat)) {
        execError = 3102;                                                           // 3102: floating point type arguments expected
        return false;                                                               // an error occurred
    }

    float* pReal1 = (float*)(pdata[0]);
    float* pIm1 = pReal1 + 1;                                                       // next array element

    float* pReal2 = (float*)(pdata[1]);
    float* pIm2 = pReal2 + 1;

    float* pRealResult = (float*)pdata[2];
    float* pImResult = pRealResult + 1;

    *pRealResult = *pReal1 + *pReal2;
    *pImResult = *pIm1 + *pIm2;

    return true;                                                                    // success
}


// ----------------------------------------------------------
// *   convert Cartesian coordinates to polar coordinates   *
// ----------------------------------------------------------
bool JustinaComplex::cmplxCtoP(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered with same Justina name):
    ----------------------------------------------------------------
    var cart(2), polar(2);              // arrays for Cartesian and polar coordinates, respectively
    ...                                 // input : cart(1) = real part, cart(2) = imaginary part
    cmplxCtoP(cart(1), polar(1));       // return: polar(1) = radius, polar(2) = angle (always pass an array element, not an array name)

*/

    // test arguments
    if (((valueType[0] & Justina::value_typeMask) != Justina::value_isFloat) ||     // floating point arrays expected
        ((valueType[1] & Justina::value_typeMask) != Justina::value_isFloat)) {
        execError = 3102;                                                           // error 3102: floating point type arguments expected
        return false;                                                               // an error occurred
    }

    float* pReal = (float*)pdata[0];
    float* pIm = pReal + 1;                                                         // next array element

    float* pRadius = (float*&)pdata[1];
    float* pAngle = pRadius + 1;

    *pRadius = sqrt(pow(*pReal, 2.) + pow(*pIm, 2));
    if (*pRadius == 0) { *pAngle = 0; }                                             // by convention
    else { *pAngle = acos(*pReal / *pRadius); }

    if (*pIm < 0) { *pAngle = TWO_PI - *pAngle; }

    return true;                                                                    // success
}
