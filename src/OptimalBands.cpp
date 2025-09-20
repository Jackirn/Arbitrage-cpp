#include "utilities/OptimalBands.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include <nlopt.hpp>

namespace util {

// ---------------- erfid_matlab ----------------
double erfid_matlab(double x, double y) {
    auto integrand = [](double t){ return std::exp(0.5 * t * t); };
    double result = boost::math::quadrature::gauss_kronrod<double, 15>::integrate(
        integrand, y, x, 5, 1e-8
    );
    return std::sqrt(2.0/M_PI) * result;
}

// ---------------- long_return ----------------
std::tuple<double,double> long_return(
    double d, double u,
    double c, double l,
    double sigma, double f
){
    if ((u - d <= c) || (d <= l) || (u <= d))
        return {-INFINITY, NAN};

    double expoUD = std::exp(sigma * (u - d - c)) - 1.0;
    double expoLD = std::exp(sigma * (l - d - c)) - 1.0;

    double fStar;
    if (!std::isnan(f)) {
        fStar = f; // leverage fisso
    } else {
        double denomUL = erfid_matlab(u, l);
        fStar = -erfid_matlab(d, l) / (expoLD * denomUL)
              - erfid_matlab(u, d) / (expoUD * denomUL);
    }

    double mu;
    try {
        mu = (2.0 / M_PI) * (
            std::log(1 + fStar * expoUD) / erfid_matlab(u, d) +
            std::log(1 + fStar * expoLD) / erfid_matlab(d, l)
        );
    } catch (...) {
        mu = -INFINITY;
    }

    return {mu, fStar};
}

// ---------------- optimal_trading_bands ----------------
OptimalBandsResult optimal_trading_bands(
    int M, double l, double f,
    double k_hat, double sigma_hat,
    double C, double alpha, int grid
){
    (void)M; (void)alpha; (void)grid; // se non usati nella tua versione attuale

    OptimalBandsResult R;
    R.f_input = f;

    const double theta      = 1.0 / k_hat;
    const double sigma_stat = sigma_hat / std::sqrt(2 * k_hat);
    const double c          = C / sigma_stat;

    // objective
    auto obj_fun = [&](const std::vector<double>& x, std::vector<double>& /*grad*/) {
        auto tup = long_return(x[0], x[1], c, l, sigma_stat, f);
        double mu = std::get<0>(tup);
        return -mu;
    };

    // bounds
    const double d_min = l + 0.01;
    const double d_max = 0.6;
    const double u_min = l + C;
    const double u_max = 3.0;

    nlopt::opt opt(nlopt::LD_SLSQP, 2);
    opt.set_lower_bounds({d_min, u_min});
    opt.set_upper_bounds({d_max, u_max});
    opt.set_min_objective(
        [](const std::vector<double>& x, std::vector<double>& grad, void* data)->double {
            auto* self = reinterpret_cast<decltype(obj_fun)*>(data);
            return (*self)(x, grad);
        }, &obj_fun
    );
    opt.set_xtol_rel(1e-8);
    opt.set_maxeval(500);

    std::vector<double> x0 = {-0.5, 0.5};
    double minf = 0.0;
    nlopt::result result = opt.optimize(x0, minf);

    if (result > 0) {
        double d = std::abs(x0[0]);
        double u = x0[1];
        auto [mu, fstar] = long_return(-d, u, c, l, sigma_stat, f);

        R.d_estimated = d;
        R.u_estimated = u;
        R.mu_estimated = mu / theta;
        if (std::isnan(f)) R.f_estimated = fstar;
    } else {
        std::cerr << "[warn] Optimization failed\n";
        return R;
    }

    // (Bootstrap CI: aggiungilo qui se/quando implementi la parte M>1)
    return R;
}

} // namespace util