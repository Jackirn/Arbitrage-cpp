#pragma once
#include <array>
#include <vector>
#include <optional>
#include <tuple>

namespace util {

    // Risultato della stima delle bande
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

    // funzione MATLAB-like
    double erfid_matlab(double x, double y);

    // ritorno di lungo periodo
    std::tuple<double,double> long_return(
        double d, double u,
        double c, double l,
        double sigma, double f
    );

    // funzione per calcolare le bande ottimali
    OptimalBandsResult optimal_trading_bands(
        int M, double l, double f,
        double k_hat, double sigma_hat,
        double C, double alpha, int grid
    );

} // namespace util