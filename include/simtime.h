//==========================================================================
//   SIMTIME.H  - part of
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//==========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 1992-2006 Andras Varga

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#ifndef __SIMTIME_H
#define __SIMTIME_H

#include <string>
#include <iostream>
#include "simkerneldefs.h"
#include "platdep/inttypes.h"

/**
 * int64-based simulation time.
 */
class SIM_API SimTime
{
  private:
    int64 t;

    static int scaleexp;     // scale exponent in the range -18..0
    static double fscale;    // 10^-scaleexp, that is 1 or 1000 or 1000000...
    static double invfscale; // 1/fscale; we store it because floating-point multiplication is faster than division

  public:
    static const int SCALEEXP_S  =  0;
    static const int SCALEEXP_MS = -3;
    static const int SCALEEXP_US = -6;
    static const int SCALEEXP_NS = -9;
    static const int SCALEEXP_PS = -12;
    static const int SCALEEXP_FS = -15;

    SimTime() {t=0;}
//XXX these integer ctors are no win -- looks like performance-wise int*int64 ~ double*int64
//    SimTime(int d)   {operator=(d);}
//XXX another idea that does NOT work for speeding up conversion from double: t = d==0 ? 0 : (int64)(fscale*d);

    SimTime(double d) {operator=(d);}
    SimTime(const SimTime& x) {t=x.t;}

    bool operator==(const SimTime& x) const  {return t==x.t;}
    bool operator!=(const SimTime& x) const  {return t!=x.t;}
    bool operator< (const SimTime& x) const  {return t<x.t;}
    bool operator> (const SimTime& x) const  {return t>x.t;}
    bool operator<=(const SimTime& x) const  {return t<=x.t;}
    bool operator>=(const SimTime& x) const  {return t>=x.t;}

    const SimTime& operator=(double d) {t = (int64)(fscale*d); return *this;}
    const SimTime& operator=(const SimTime& x) {t=x.t; return *this;}

    const SimTime& operator+=(const SimTime& x) {t+=x.t; return *this;}
    const SimTime& operator-=(const SimTime& x) {t-=x.t; return *this;}

    const SimTime& operator*=(double d) {t*=d; return *this;} //XXX to be checked on Linux, see below
    const SimTime& operator/=(double d) {t/=d; return *this;} //XXX to be checked on Linux, see below

    friend const SimTime operator+(const SimTime& x, const SimTime& y);
    friend const SimTime operator-(const SimTime& x, const SimTime& y);
    friend const SimTime operator*(const SimTime& x, double d);
    friend const SimTime operator*(double d, const SimTime& x);
    friend const SimTime operator/(const SimTime& x, double d);
    friend double operator/(const SimTime& x, const SimTime& y);

    /**
     * Converts simulation time to double. Note that conversion to and from
     * double may lose precision. We do not provide implicit conversion to
     * double as it would conflict with other overloaded operators, and would
     * cause ambiguities and unexpected conversions to be chosen by the
     * C++ compiler.
     */
     double dbl() const  {return t*invfscale;}

    /**
     * Comparison functions. They provide a performance advantage by avoiding
     * conversion to double, and are more concise than comparing against
     * <tt>SimTime::zero()</tt>.
     */
    //@{
    bool isZero()     {return t!=0;}
    bool isPositive() {return t>0;}
    bool isNegative() {return t<0;}
    bool isMaxTime()  {return t==maxTime().t;}
    //@}

    std::string str() const;

    int64 raw() const  {return t;}
    const SimTime& setRaw(int64 l) {t = l; return *this;}

    /**
     * Returns 0 as simulation time. The code <tt>t = SimTime::zero();</tt> or
     * <tt>t = SimTime();</tt> is more efficient than plain <tt>t = 0;</tt>,
     * because the latter involves a <tt>SimTime(double)</tt> constructor call
     * which the compiler usually does not fully optimize.
     */
    static const SimTime zero() {return SimTime();}

    /**
     * Returns the largest simulation time that can be represented using the
     * present scale exponent.
     */
    static const SimTime maxTime() {return SimTime().setRaw((((int64)1) << 63) - 1);}

    /**
     * Returns the time resolution as the number of units per second,
     * e.g. for microsecond resolution it returns 1000000.
     */
    static int64 scale()  {return (int64)fscale;}

    /**
     * Returns the scale exponent, which is an integer in the range -18..0.
     * For example, for microsecond resolution it returns -6.
     */
    static int scaleExp() {return scaleexp;}

    /**
     * Sets the scale exponent, and thus the resolution of time. Accepted
     * values are -18..0; for example, setScaleExp(-6) selects microsecond
     * resolution.
     *
     * IMPORTANT: This function has a global effect, and therefore
     * should NEVER be called during simulation.
     */
    static void setScaleExp(int e);

};

/*XXX
 for *= and /=, SystemC uses the following code:
    // linux bug workaround; don't change next two lines
    volatile double tmp = uint64_to_double( m_value ) * d;
    m_value = static_cast<int64>( tmp );
    return *this;
*/

inline const SimTime operator+(const SimTime& x, const SimTime& y)
{
    return SimTime(x)+=y;
}

inline const SimTime operator-(const SimTime& x, const SimTime& y)
{
    return SimTime(x)-=y;
}

inline const SimTime operator*(const SimTime& x, double d)
{
    return SimTime(x)*=d;
}

inline const SimTime operator*(double d, const SimTime& x)
{
    return SimTime(x)*=d;
}

inline const SimTime operator/(const SimTime& x, double d)
{
    return SimTime(x)/=d;
}

inline double operator/(const SimTime& x, const SimTime& y)
{
    return (double)x.t / (double)y.t;
}

inline std::ostream& operator<<(std::ostream& os, const SimTime& x)
{
    return os << x.dbl() << " (raw:" << x.raw() << ")";   //XXX refine
}

#endif
