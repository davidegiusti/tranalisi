#ifndef _MATH_HPP
#define _MATH_HPP

#include <type_traits>

using namespace std;

//! helper for unary minus
template <class T> T uminus(const T &in)
{return -in;}

//! define the square of a double, float or integer
template <class T> T sqr(const T &x)
{return x*x;}

//! define the cube
template <class T> T cube(const T &x)
{return x*x*x;}

#endif
