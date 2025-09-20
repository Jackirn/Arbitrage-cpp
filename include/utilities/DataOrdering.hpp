#pragma once
#include <vector>
#include <string>
#include <optional>
#include <utility> // std::pair

namespace util {

    // Riga della tabella (nomi esattamente come usati nel .cpp)
    struct PriceRow {
        std::string Time;   // "YYYY-MM-DD HH:MM:SS"
        double Bid1{0.0}, Ask1{0.0}, Mid1{0.0};
        double Bid2{0.0}, Ask2{0.0}, Mid2{0.0};
        double Rt{0.0};     // log(Mid1/Mid2)
    };

    using PriceTable = std::vector<PriceRow>;

    // ---- dichiarazioni funzioni ----
    PriceTable build_price_table(
        const std::vector<std::string>& time,
        std::optional<std::vector<double>> bid1,
        std::optional<std::vector<double>> ask1,
        std::optional<std::vector<double>> mid1,
        std::optional<double> tick1,
        double conv1,
        std::optional<std::vector<double>> bid2,
        std::optional<std::vector<double>> ask2,
        std::optional<std::vector<double>> mid2,
        std::optional<double> tick2,
        double conv2,
        const std::optional<std::string>& start_date = std::nullopt,
        const std::optional<std::string>& end_date   = std::nullopt
    );

    std::pair<PriceTable, PriceTable> trim_and_split_price_table(
        const PriceTable& data,
        std::optional<double> IS_start_hour = std::nullopt,
        std::optional<double> IS_end_hour   = std::nullopt,
        std::optional<double> OS_start_hour = std::nullopt,
        std::optional<double> OS_end_hour   = std::nullopt,
        int split_months = 0
    );

    std::pair<PriceTable, PriceTable> split_price_table_by_months(
        const PriceTable& data,
        int split_months
    );

    struct OutlierResult {
        PriceTable clean;
        std::vector<bool> is_outlier;
        PriceTable outliers;
    };

    OutlierResult filter_log_spread_outliers(const PriceTable& data);
    OutlierResult filter_antipersistent_outliers(const PriceTable& data);
    OutlierResult remove_outliers(const PriceTable& data);

} // namespace util