#include <iostream>
#include <fstream>
#include <array>
#include <vector>
#include <optional>
#include <algorithm>
#include <cmath>
#include <limits>
#include <iomanip>
#include <string>
#include <sstream>

#include "utilities/DataOrdering.hpp"
#include "utilities/Loaders.hpp"
#include "utilities/StatisticalBootstrap.hpp"
#include "utilities/OptimalBands.hpp"

int main() {
    using namespace util;

    try {
        std::cout << "=== Arbitrage C++ Pipeline ===\n";

        // ====== 1) LOAD ======
        const std::string csv_path = "HO-LGO.csv";
        const std::string time_col = "*"; // auto-detect "Timestamp" o "..._Timestamp"

        const std::array<std::string,4> bid_ask_cols{
            "HOc2_Bid Close",
            "HOc2_Ask Close",
            "LGOc6_Bid Close",
            "LGOc6_Ask Close"
        };

        const std::optional<std::array<std::string,2>> mid_cols = std::nullopt;
        const std::optional<std::array<double,2>>     ticks    = std::nullopt;

        // conversioni unitarie: HO * 42, LGO * (1/7.44)
        const std::array<double,2> convs{42.0, 1.0/7.44};

        // filtri data (start incluso, end escluso)
        const std::optional<std::string> start_date = "2015-04-22";
        const std::optional<std::string> end_date   = "2016-04-22";

        auto tbl = load_and_process_price_data_csv(
            csv_path, time_col, bid_ask_cols, mid_cols, ticks, convs, start_date, end_date
        );

        std::cout << "Righe caricate: " << tbl.size() << "\n";
        for (size_t i=0; i<std::min<size_t>(5, tbl.size()); ++i){
            const auto& r = tbl[i];
            std::cout << r.Time
                      << " | Mid1=" << r.Mid1
                      << " | Mid2=" << r.Mid2
                      << " | Rt="   << r.Rt << "\n";
        }
        if (tbl.empty()) {
            std::cerr << "[Errore] Nessuna riga caricata: controlla percorso CSV e nomi colonne.\n";
            return 1;
        }

        // ====== 2) TRIM & SPLIT ======
        auto [data_IS_8_16, data_OS] = trim_and_split_price_table(
            tbl, /*IS 8-16*/ 8, 16, /*OS exclude 17-20*/ 17, 20, /*split months*/ 9
        );
        auto [data_IS_9_16, _trash] = trim_and_split_price_table(
            tbl, /*IS 9-16*/ 9, 16, /*OS exclude 17-20*/ 17, 20, /*split months*/ 9
        );

        // ====== 3) OUTLIERS ======
        auto out_IS_8_16 = remove_outliers(data_IS_8_16);
        auto out_IS_9_16 = remove_outliers(data_IS_9_16);
        auto out_OS      = remove_outliers(data_OS);

        const auto& clean_IS_8_16 = out_IS_8_16.clean;
        const auto& clean_IS_9_16 = out_IS_9_16.clean;
        const auto& clean_OS      = out_OS.clean;

        std::cout << "\n[Info] IS(8-16) size: " << data_IS_8_16.size()
                  << " -> clean: " << clean_IS_8_16.size()
                  << " | outliers: " << out_IS_8_16.outliers.size() << "\n";
        std::cout << "[Info] IS(9-16) size: " << data_IS_9_16.size()
                  << " -> clean: " << clean_IS_9_16.size()
                  << " | outliers: " << out_IS_9_16.outliers.size() << "\n";
        std::cout << "[Info] OS size: " << data_OS.size()
                  << " -> clean: " << clean_OS.size()
                  << " | outliers: " << out_OS.outliers.size() << "\n";

        // ====== 4) BOOTSTRAP OU ======
        // IS 8-16: M=1000
        {
            int    M_boot  = 1000;
            double alphaCI = 0.05;   // 95% CI
            uint64_t seed  = 42;

            auto R = stats::ou_bootstrap(clean_IS_8_16, M_boot, alphaCI, seed);
            std::cout << "\nEstimates for IS dataset (8-16):\n";
            stats::print_ou_estimates(R);
        }

        // IS 9-16: M=10000
        stats::OUBootstrapResult R_9_16;
        {
            int    M_boot  = 10000;
            double alphaCI = 0.05;   // 95% CI
            uint64_t seed  = 42;

            R_9_16 = stats::ou_bootstrap(clean_IS_9_16, M_boot, alphaCI, seed);
            std::cout << "\nEstimates for IS dataset (9-16):\n";
            stats::print_ou_estimates(R_9_16);
        }

        // ====== 5) COSTO DI TRANSAZIONE MEDIO (su IS 9-16)
        double C = 0.0;
        {
            double sum = 0.0;
            size_t count = 0;
            for (const auto& r : data_IS_9_16) { // come nel Python: dataset grezzo IS 9–16
                if (r.Bid1 > 0.0 && r.Ask1 > 0.0 && r.Bid2 > 0.0 && r.Ask2 > 0.0) {
                    double ct = std::log(r.Ask1 / r.Bid1) + std::log(r.Ask2 / r.Bid2);
                    if (std::isfinite(ct)) { sum += ct; ++count; }
                }
            }
            if (count == 0) {
                std::cerr << "[Warn] Nessuna osservazione valida per il costo C; metto C=0.\n";
                C = 0.0;
            } else {
                C = sum / static_cast<double>(count);
            }
            std::cout << "\n[Info] C (avg log-transaction cost) = " << C << " (n=" << count << ")\n";
        }

        // ====== 6) OPTIMAL BANDS sweep su l e f ======
        const std::vector<double> l_list = {-1.282, -1.645, -1.96, -2.326};

        struct FCase { std::string label; double value; };
        const std::vector<FCase> f_list = {
            {"1",   1.0},
            {"2",   2.0},
            {"5",   5.0},
            {"opt", std::numeric_limits<double>::quiet_NaN()} // NaN => calcola f*
        };

        // Prendi i point estimates OU dal bootstrap IS 8–16
        // PRIMA (sbagliato se i campi non hanno _hat)
        // ====== 4) BOOTSTRAP OU ======
        // IS 8-16: M=1000
        stats::OUBootstrapResult R_8_16;
        {
            int    M_boot  = 1000;
            double alphaCI = 0.05;   // 95% CI
            uint64_t seed  = 42;

            R_8_16 = stats::ou_bootstrap(clean_IS_8_16, M_boot, alphaCI, seed);
            std::cout << "\nEstimates for IS dataset (8-16):\n";
            stats::print_ou_estimates(R_8_16);
        }
        double k_hat     = R_8_16.k;
        double sigma_hat = R_8_16.sigma;

        int    M_opt  = 100000;
        double alpha  = 0.05;
        int    grid   = 100;

        auto fmt6 = [](double x)->std::string{
            std::ostringstream oss;
            if (std::isfinite(x)) { oss << std::fixed << std::setprecision(6) << x; }
            return oss.str();
        };

        struct Row {
            double l;
            std::string f_label;
            double d_star, d_low, d_high;
            double u_star, u_low, u_high;
            double mu, mu_low, mu_high;
            std::string f_star_str;
            std::string f_low_str, f_high_str;
        };
        std::vector<Row> results;

        for (double l : l_list) {
            for (const auto& fcase : f_list) {
                auto Rbands = util::optimal_trading_bands(
                    M_opt, l, fcase.value,
                    k_hat, sigma_hat,
                    C, alpha, grid
                );

                Row row;
                row.l       = l;
                row.f_label = fcase.label;
                row.d_star  = Rbands.d_estimated;
                row.d_low   = Rbands.d_CI[0];
                row.d_high  = Rbands.d_CI[1];
                row.u_star  = Rbands.u_estimated;
                row.u_low   = Rbands.u_CI[0];
                row.u_high  = Rbands.u_CI[1];
                row.mu      = Rbands.mu_estimated;
                row.mu_low  = Rbands.mu_CI[0];
                row.mu_high = Rbands.mu_CI[1];

                if (fcase.label == "opt") {
                    row.f_star_str = std::isfinite(Rbands.f_estimated) ? fmt6(Rbands.f_estimated) : "";
                    row.f_low_str  = std::isfinite(Rbands.f_opt_CI[0]) ? fmt6(Rbands.f_opt_CI[0]) : "";
                    row.f_high_str = std::isfinite(Rbands.f_opt_CI[1]) ? fmt6(Rbands.f_opt_CI[1]) : "";
                } else {
                    row.f_star_str = "";
                    row.f_low_str  = "";
                    row.f_high_str = "";
                }

                results.push_back(std::move(row));
            }
        }

        // ====== 7) Stampa tabella risultati ======
        std::cout << "\n=== Optimal Bands Results ===\n";
        std::cout << std::left
                  << std::setw(10) << "Stop-loss"
                  << std::setw(10) << "Leverage"
                  << std::setw(12) << "d*"
                  << std::setw(12) << "d_CI_low"
                  << std::setw(12) << "d_CI_high"
                  << std::setw(12) << "u*"
                  << std::setw(12) << "u_CI_low"
                  << std::setw(12) << "u_CI_high"
                  << std::setw(12) << "mu"
                  << std::setw(12) << "mu_CI_low"
                  << std::setw(12) << "mu_CI_high"
                  << std::setw(10) << "f*"
                  << std::setw(12) << "f_CI_low"
                  << std::setw(12) << "f_CI_high"
                  << "\n";

        auto fmt = [&](double x){ return fmt6(x); };

        for (const auto& r : results) {
            std::cout << std::left
                      << std::setw(10) << fmt(r.l)
                      << std::setw(10) << r.f_label
                      << std::setw(12) << fmt(r.d_star)
                      << std::setw(12) << fmt(r.d_low)
                      << std::setw(12) << fmt(r.d_high)
                      << std::setw(12) << fmt(r.u_star)
                      << std::setw(12) << fmt(r.u_low)
                      << std::setw(12) << fmt(r.u_high)
                      << std::setw(12) << fmt(r.mu)
                      << std::setw(12) << fmt(r.mu_low)
                      << std::setw(12) << fmt(r.mu_high)
                      << std::setw(10) << r.f_star_str
                      << std::setw(12) << r.f_low_str
                      << std::setw(12) << r.f_high_str
                      << "\n";
        }

        // ====== 8) Salva CSV risultati ======
        {
            std::ofstream fout("outputs/optimal_bands_results.csv");
            if (!fout.is_open()) {
                std::cerr << "[Warn] impossibile aprire outputs/optimal_bands_results.csv per scrivere.\n";
            } else {
                fout << "Stop-loss,Leverage,d*,d_CI_low,d_CI_high,u*,u_CI_low,u_CI_high,mu,mu_CI_low,mu_CI_high,f*,f_CI_low,f_CI_high\n";
                for (const auto& r : results) {
                    fout << fmt(r.l) << ","
                         << r.f_label << ","
                         << fmt(r.d_star) << ","
                         << fmt(r.d_low)  << ","
                         << fmt(r.d_high) << ","
                         << fmt(r.u_star) << ","
                         << fmt(r.u_low)  << ","
                         << fmt(r.u_high) << ","
                         << fmt(r.mu)     << ","
                         << fmt(r.mu_low) << ","
                         << fmt(r.mu_high)<< ","
                         << r.f_star_str  << ","
                         << r.f_low_str   << ","
                         << r.f_high_str  << "\n";
                }
                std::cout << "\n[Info] Salvato: outputs/optimal_bands_results.csv\n";
            }
        }

        std::cout << "\n[OK] Fine pipeline.\n";
        return 0;

    } catch (const std::exception& ex) {
        std::cerr << "Errore: " << ex.what() << "\n";
        return 1;
    }
}