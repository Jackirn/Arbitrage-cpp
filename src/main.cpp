#include <iostream>
#include <array>
#include <vector>
#include <cmath>
#include "utilities/DataOrdering.hpp"
#include "utilities/Loaders.hpp"
#include "utilities/OptimalBands.hpp"

int main() {
    using namespace util;

    try {
        // ===== 1) CARICAMENTO DATI DAL CSV =====
        auto tbl = load_and_process_price_data_csv(
            "HO-LGO.csv",
            "*", // auto-detect colonna tempo
            std::array<std::string,4>{
                "HOc2_Bid Close",
                "HOc2_Ask Close",
                "LGOc6_Bid Close",
                "LGOc6_Ask Close"
            },
            std::nullopt,                      // nessuna colonna mid esplicita
            std::nullopt,                      // nessun tick
            std::array<double,2>{42.0, 1.0/7.44}, // conversioni come in Python
            "2015-04-22",  // start incluso
            "2016-04-22"   // end escluso
        );

        std::cout << "Righe caricate: " << tbl.size() << "\n";
        for (size_t i=0; i<std::min<size_t>(5, tbl.size()); ++i){
            const auto& r = tbl[i];
            std::cout << r.Time
                      << " | Mid1=" << r.Mid1
                      << " | Mid2=" << r.Mid2
                      << " | Rt="   << r.Rt << "\n";
        }

        // (opzionale) Se vuoi replicare la logica Python:
        // split per finestre orarie e primi 9 mesi + filtri outlier
        auto [data_IS_8_16, data_OS] = trim_and_split_price_table(
            tbl,
            /*IS_start_hour*/ 8.0, /*IS_end_hour*/ 16.0,
            /*OS_start_hour*/ 17.0, /*OS_end_hour*/ 20.0,
            /*split_months*/ 9
        );
        auto [clean_IS_8_16, outliers_IS_8_16, mask1] = remove_outliers(data_IS_8_16);
        auto [clean_OS,      outliers_OS,      mask2] = remove_outliers(data_OS);

        std::cout << "\n[Info] IS(8-16) size: " << data_IS_8_16.size()
                  << " -> clean: " << clean_IS_8_16.size()
                  << " | outliers: " << outliers_IS_8_16.size() << "\n";
        std::cout << "[Info] OS size: " << data_OS.size()
                  << " -> clean: " << clean_OS.size()
                  << " | outliers: " << outliers_OS.size() << "\n\n";

        // ===== 2) DEMO OPTIMAL BANDS =====
        // Per ora usiamo parametri OU fittizi finché non colleghiamo il bootstrap:
        int    M      = 1;       // no bootstrap
        double l      = -1.96;   // stop-loss (in sigma-units)
        double f      = 1.0;     // leverage fisso
        double alpha  = 0.95;
        int    grid   = 100;
        double k_val  = 1.0;
        double sigma_val = 1.0;

        int n_points = 100;
        double c_sigma_max = 0.76;
        std::vector<double> c_values(n_points);
        for (int i = 0; i < n_points; i++) {
            // come in Python: C naturale = (c_sigma * sigma) / sqrt(2k)
            c_values[i] = i * (c_sigma_max * sigma_val / std::sqrt(2.0 * k_val)) / (n_points - 1);
        }

        std::vector<double> d_opt(n_points), u_opt(n_points);

        std::cout << "=== Optimal Bands (demo k=1, sigma=1) ===\n";
        for (int i = 0; i < n_points; i++) {
            double C = c_values[i];
            auto R = optimal_trading_bands(/*M   */ M,
                                           /*l   */ l,
                                           /*f   */ f,
                                           /*k   */ k_val,
                                           /*sig */ sigma_val,
                                           /*C   */ C,
                                           /*alp */ alpha,
                                           /*grid*/ grid);
            d_opt[i] = R.d_estimated;
            u_opt[i] = R.u_estimated;

            // stampa qualche punto per controllo
            if (i % 10 == 0 || i == n_points-1) {
                std::cout << "C=" << C
                          << " -> d*=" << R.d_estimated
                          << ", u*="   << R.u_estimated
                          << ", mu="   << R.mu_estimated
                          << "\n";
            }
        }

        // Nota: se vuoi, qui puoi salvare d_opt/u_opt su CSV per farne il plot in Python/Excel.

    } catch (const std::exception& ex){
        std::cerr << "Errore: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}