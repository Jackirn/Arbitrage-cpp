#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>
#include "DataOrdering.hpp"

namespace util {

    /**
     * Carica un CSV con due righe di header (Excel-style), fa ffill sulla 1ª riga,
     * combina "row1_row2", gestisce ','/';' e numeri con virgola.
     *
     * Se time_col == "*" tenta auto-detect: "Timestamp" oppure "*_Timestamp".
     */
    PriceTable load_and_process_price_data_csv(
        const std::string& filepath,
        const std::string& time_col,                       // oppure "*" per auto-detect
        const std::array<std::string,4>& bid_ask_cols,     // {Bid1, Ask1, Bid2, Ask2}
        const std::optional<std::array<std::string,2>>& mid_cols,
        const std::optional<std::array<double,2>>& ticks,
        const std::array<double,2>& convs,
        const std::optional<std::string>& start_date = std::nullopt,
        const std::optional<std::string>& end_date   = std::nullopt
    );

}