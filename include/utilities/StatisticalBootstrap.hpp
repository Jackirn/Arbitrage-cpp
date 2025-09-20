#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <string>

#include "utilities/DataOrdering.hpp"  // per util::PriceTable

namespace stats {

    struct OUBootstrapResult {
        // point estimates (MLE) sul dataset passato
        double k   = 0.0;
        double eta = 0.0;
        double sigma = 0.0;

        // campioni bootstrap (M elementi)
        std::vector<double> boot_k;
        std::vector<double> boot_eta;
        std::vector<double> boot_sigma;

        // intervalli di confidenza (lower, upper)
        std::array<double,2> CI_k   {NAN, NAN};
        std::array<double,2> CI_eta {NAN, NAN};
        std::array<double,2> CI_sigma{NAN, NAN};
    };

    // Stima MLE + bootstrap parametrico OU sul campo Rt del PriceTable pulito
    OUBootstrapResult ou_bootstrap(const util::PriceTable& clean_data,
                                   int M = 1000,
                                   double alpha = 0.05,
                                   std::uint64_t seed = 42);

    // stampa formattata delle stime e CI
    void print_ou_estimates(const OUBootstrapResult& R);

} // namespace stats