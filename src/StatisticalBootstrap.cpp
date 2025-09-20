#include "utilities/StatisticalBootstrap.hpp"
#include <cmath>
#include <random>
#include <algorithm>
#include <iostream>
#include <vector>

namespace {

// MLE chiuso per OU su griglia equispaziata
static void ou_mle(const std::vector<double>& x, double dt,
                   double& k, double& eta, double& sigma)
{
    const size_t Np1 = x.size();
    if (Np1 < 3) { k=eta=sigma=0.0; return; }

    const size_t N = Np1 - 1;
    double sum_m=0, sum_p=0, sum_mm=0, sum_pp=0, sum_pm=0;
    for (size_t i=0;i<N;i++){
        double xm = x[i], xp = x[i+1];
        sum_m  += xm;
        sum_p  += xp;
        sum_mm += xm*xm;
        sum_pp += xp*xp;
        sum_pm += xm*xp;
    }
    double Y_m  = sum_m / N;
    double Y_p  = sum_p / N;
    double Y_mm = sum_mm / N;
    double Y_pp = sum_pp / N;
    double Y_pm = sum_pm / N;

    double denom = (Y_mm - Y_m*Y_m);
    double rho   = (denom != 0.0) ? (Y_pm - Y_m*Y_p)/denom : 0.0;
    if (rho <= 0.0) rho = 1e-8; // evita log di <=0
    if (rho >= 1.0) rho = 1.0 - 1e-8;

    k = -std::log(rho) / dt;

    // stima semplice di eta (non essenziale per le bande)
    eta = Y_p + ((x.back() - x.front())/static_cast<double>(N)) *
                (Y_pm - Y_m*Y_p) /
                std::max(1e-12, (Y_mm - Y_m*Y_m) - (Y_pm - Y_m*Y_p));

    double sigma2 = Y_pp - Y_p*Y_p
                  - ( (Y_pm - Y_m*Y_p)*(Y_pm - Y_m*Y_p) ) / std::max(1e-12, denom);
    sigma2 = std::max(sigma2, 1e-12);
    sigma  = std::sqrt( (2.0*k*sigma2) / (1.0 - std::exp(-2.0*k*dt)) );
}

// simulazione esatta 1-step OU
static std::vector<double> ou_sim(double x0, double k, double eta, double sigma,
                                  double dt, size_t N, std::mt19937_64& rng)
{
    std::normal_distribution<double> Z(0.0, 1.0);
    std::vector<double> x(N+1);
    x[0] = x0;

    const double a  = std::exp(-k*dt);
    const double b  = eta * (1.0 - a);
    const double sd = sigma * std::sqrt( (1.0 - a*a) / (2.0*k) );

    for (size_t i=0;i<N;i++){
        x[i+1] = a*x[i] + b + sd*Z(rng);
    }
    return x;
}

static double percentile(std::vector<double> v, double p01_99){
    if (v.empty()) return NAN;
    std::sort(v.begin(), v.end());
    double pos = (p01_99/100.0)*(v.size()-1);
    size_t i = static_cast<size_t>(std::floor(pos));
    size_t j = static_cast<size_t>(std::ceil(pos));
    double w = pos - i;
    return (1.0-w)*v[i] + w*v[j];
}

} // anon

namespace stats {

OUBootstrapResult ou_bootstrap(const util::PriceTable& clean_data,
                               int M, double alpha, std::uint64_t seed)
{
    OUBootstrapResult R;

    // estrai serie Rt e tempi per dt medio (anni)
    if (clean_data.size() < 3) return R;

    std::vector<double> x;
    x.reserve(clean_data.size());
    for (const auto& r : clean_data) x.push_back(r.Rt);

    // dt medio (approssimiamo a campionamento regolare sull’intervallo totale)
    // NB: qui non parseiamo le date; assumiamo 30 min come nel tuo dataset: 0.5/24/365 anni
    // Se vuoi preciso, calcola da stringhe.
    const double dt = (0.5/24.0)/365.0;

    // MLE sui dati reali
    ou_mle(x, dt, R.k, R.eta, R.sigma);

    // bootstrap parametrico
    std::mt19937_64 rng(seed);
    R.boot_k.reserve(M);
    R.boot_eta.reserve(M);
    R.boot_sigma.reserve(M);

    for (int m=0; m<M; ++m){
        auto xs = ou_sim(x.front(), R.k, R.eta, R.sigma, dt, x.size()-1, rng);
        double k, eta, sigma;
        ou_mle(xs, dt, k, eta, sigma);
        R.boot_k.push_back(k);
        R.boot_eta.push_back(eta);
        R.boot_sigma.push_back(sigma);
    }

    double lowp  = alpha*50.0;
    double highp = 100.0 - alpha*50.0;
    R.CI_k[0]     = percentile(R.boot_k, lowp);
    R.CI_k[1]     = percentile(R.boot_k, highp);
    R.CI_eta[0]   = percentile(R.boot_eta, lowp);
    R.CI_eta[1]   = percentile(R.boot_eta, highp);
    R.CI_sigma[0] = percentile(R.boot_sigma, lowp);
    R.CI_sigma[1] = percentile(R.boot_sigma, highp);

    return R;
}

void print_ou_estimates(const OUBootstrapResult& R){
    std::cout << "Ornstein-Uhlenbeck Parameter Estimates\n"
              << "---------------------------------------------\n";
    std::cout << "k     : Estimate = " << R.k
              << ", 95% CI = [" << R.CI_k[0] << ", " << R.CI_k[1] << "]\n";
    std::cout << "eta   : Estimate = " << R.eta
              << ", 95% CI = [" << R.CI_eta[0] << ", " << R.CI_eta[1] << "]\n";
    std::cout << "sigma : Estimate = " << R.sigma
              << ", 95% CI = [" << R.CI_sigma[0] << ", " << R.CI_sigma[1] << "]\n";
}

} // namespace stats