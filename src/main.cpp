#include <iostream>
#include <array>
#include "utilities/DataOrdering.hpp"
#include "utilities/Loaders.hpp"

int main() {
    using namespace util;

    try {
        auto tbl = load_and_process_price_data_csv(
            "HO-LGO.csv",
            "*",
            std::array<std::string,4>{
                "HOc2_Bid Close",
                "HOc2_Ask Close",
                "LGOc6_Bid Close",
                "LGOc6_Ask Close"
            },
            std::nullopt, std::nullopt,
            std::array<double,2>{42.0, 1.0/7.44},
            "2015-04-22",   // start incluso
            "2016-04-22"    // end escluso
        );

        std::cout << "Righe caricate: " << tbl.size() << "\n";
        for (size_t i=0; i<std::min<size_t>(5, tbl.size()); ++i){
            const auto& r = tbl[i];
            std::cout << r.Time << " | Mid1=" << r.Mid1 << " | Mid2=" << r.Mid2 << " | Rt=" << r.Rt << "\n";
        }
    } catch (const std::exception& ex){
        std::cerr << "Errore: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}