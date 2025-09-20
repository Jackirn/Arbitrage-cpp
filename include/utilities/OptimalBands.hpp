#pragma once
#include <array>
#include <tuple>

namespace util {

    struct OptimalBandsResult {
        double d_estimated = NAN;
        double u_estimated = NAN;
        double mu_estimated = NAN;
        std::array<double,2> d_CI{NAN, NAN};
        std::array<double,2> u_CI{NAN, NAN};
        std::array<double,2> mu_CI{NAN, NAN};
        double f_estimated = NAN;
        std::array<double,2> f_opt_CI{NAN, NAN};
        double f_input = NAN;
    };

    // MATLAB-like integral  sqrt(2/pi) * ∫_y^x exp(t^2/2) dt
    double erfid_matlab(double x, double y);

    // Long-run return μ and chosen leverage f*
    std::tuple<double,double> long_return(
        double d, double u,
        double c, double l,
        double sigma, double f
    );

    // Compute optimal trading bands (NLopt)
    OptimalBandsResult optimal_trading_bands(
        int M, double l, double f,
        double k_hat, double sigma_hat,
        double C, double alpha, int grid
    );

} // namespace util