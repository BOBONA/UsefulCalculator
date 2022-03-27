#include <iostream>

#include <mpirxx.h>

#include "Math.h"

mpf_class RootPrecision = 0.00000000001;
mp_exp_t printExp2 = 1;

mpf_class nthRoot(mpf_class num, mpz_class exp) {
    mpf_class x = num/2;
    x.set_prec(128);
    mpz_class c1 = exp - 1;
    mpf_class c2;
    mpf_set_z(c2.get_mpf_t(), c1.get_mpz_t()); // awkward conversion
    c2 /= exp;
    mpf_class c3 = num / exp;

    mpf_class dx;
    do {
        std::string str = x.get_str(printExp2, 10, 0);
        mpf_class temp;
        mpf_pow_ui(temp.get_mpf_t(), x.get_mpf_t(), c1.get_ui());
        mpf_class xNew = c2 * x + c3 / temp;
        dx = abs(x - xNew);
        x = xNew;
    } while (dx > RootPrecision);
    std::string str = x.get_str(printExp2, 10, 0);
    x.set_prec(64);
    return x;
}