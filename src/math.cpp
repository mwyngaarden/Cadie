#include <cassert>
#include <cmath>
#include "math.h"
using namespace std;

namespace math {

double phi(double x)
{
    return 0.5 * erfc(-x / sqrt(2.0));
}

double binomial_cdf(double s, double t, double p)
{
    assert(s <= t && t > 0.0 && p > 0.0 && p < 1.0);

    double f = s / t;
    double v = p * (1.0 - p) / t;
    double x = (f - p) / sqrt(v);

    return phi(x);
}

}
