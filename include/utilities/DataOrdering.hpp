#pragma once
#include <string>
#include <vector>
#include <utility>
#include <optional>

using namespace std;

namespace util {
    string Time;
    double Bid1, Ask1, Mid1;
    double Bid2, Ask2, Mid2;
    double Rt;                // Log spread
}

using PriceTable = vector<PriceRow>; // Alias

// Estrae ora + frazione (es. 13.5 = 13:30) da "YYYY-MM-DD HH:MM:SS"
double extract_decimal_hour(const string& iso_time);

// Aggiunge mesi ad una data "YYYY-MM-DD HH:MM:SS" e restituisce una stringa iso
string add_months_iso(const string& iso_time, int months);

// Confronto data-ora (iso strings). true se a < b
bool iso_less(const string& a, const string& b);

// Vecchio DataFrame di Python

PriceTable build_price_table(
    const std::vector<std::string>& time,
    // Prodotto 1
    std::optional<std::vector<double>> bid1,
    std::optional<std::vector<double>> ask1,
    std::optional<std::vector<double>> mid1,
    std::optional<double> tick1,
    double conv1,
    // Prodotto 2
    std::optional<std::vector<double>> bid2,
    std::optional<std::vector<double>> ask2,
    std::optional<std::vector<double>> mid2,
    std::optional<double> tick2,
    double conv2,
    // Filtro date opzionale (ISO chiuse a sinistra, aperte a destra come in Python)
    const std::optional<std::string>& start_date = std::nullopt,
    const std::optional<std::string>& end_date   = std::nullopt
);

// Taglia per ore IS/OS e split per mesi.
// Ritorna (data_IS, data_OS) come in Python.
std::pair<PriceTable, PriceTable> trim_and_split_price_table(
    const PriceTable& data,
    std::optional<double> IS_start_hour = std::nullopt,
    std::optional<double> IS_end_hour   = std::nullopt,
    std::optional<double> OS_start_hour = std::nullopt,
    std::optional<double> OS_end_hour   = std::nullopt,
    int split_months = 0
);

// Solo split per mesi (senza finestre orarie).
std::pair<PriceTable, PriceTable> split_price_table_by_months(
    const PriceTable& data,
    int split_months
);

// Outlier sul log-spread Rt (IQR * 3)
struct OutlierResult {
    PriceTable clean;
    std::vector<bool> is_outlier; // stessa size dell’input
    PriceTable outliers;
};
OutlierResult filter_log_spread_outliers(const PriceTable& data);

// Outlier “antipersistent” (regola 3-punti)
OutlierResult filter_antipersistent_outliers(const PriceTable& data);

// Combina i due filtri come in Python: prima IQR, poi antipersistenti.
OutlierResult remove_outliers(const PriceTable& data);

}