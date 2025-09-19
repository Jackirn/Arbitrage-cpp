#include "utilities/DataOrdering.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace util {

static bool parse_iso(const std::string& s, std::tm& tm_out){
    // atteso: "YYYY-MM-DD HH:MM:SS"
    std::istringstream ss(s);
    ss >> std::get_time(&tm_out, "%Y-%m-%d %H:%M:%S");
    return !ss.fail();
}

double extract_decimal_hour(const std::string& iso_time){
    std::tm tm{};
    if (!parse_iso(iso_time, tm)) return NAN;
    return double(tm.tm_hour) + double(tm.tm_min)/60.0 + double(tm.tm_sec)/3600.0;
}

std::string add_months_iso(const std::string& iso_time, int months){
    std::tm tm{};
    if (!parse_iso(iso_time, tm)) return iso_time;

    int y = tm.tm_year + 1900;
    int m = tm.tm_mon + 1;
    int d = tm.tm_mday;

    int total = (y * 12 + (m - 1)) + months;
    int ny = total / 12;
    int nm = (total % 12) + 1;

    // clamp day to month length (semplice, senza calendari speciali)
    auto days_in_month = [](int year, int month){
        static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int dim = mdays[month-1];
        // bisestile
        bool leap = ( (year%4==0 && year%100!=0) || (year%400==0) );
        if (month==2 && leap) dim = 29;
        return dim;
    };
    d = std::min(d, days_in_month(ny, nm));

    std::tm out{};
    out.tm_year = ny - 1900;
    out.tm_mon  = nm - 1;
    out.tm_mday = d;
    out.tm_hour = tm.tm_hour;
    out.tm_min  = tm.tm_min;
    out.tm_sec  = tm.tm_sec;

    std::ostringstream oss;
    oss << std::put_time(&out, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// confronto lessicografico sicuro su ISO (sufficiente per stesso formato)
bool iso_less(const std::string& a, const std::string& b){
    return a < b;
}

// ---------------------------
// Utilità interne
// ---------------------------

static std::vector<double> to_conv(const std::optional<std::vector<double>>& v, double conv){
    if (!v) return {};
    std::vector<double> out = *v;
    for (auto& x : out) x *= conv;
    return out;
}

static std::vector<double> make_from_mid_with_tick(const std::vector<double>& mid, double tick, bool is_bid){
    std::vector<double> out; out.reserve(mid.size());
    const double half = tick/2.0;
    for (double m : mid) out.push_back(is_bid ? (m - half) : (m + half));
    return out;
}

static PriceTable assemble(const std::vector<std::string>& time,
                           const std::vector<double>& bid1,
                           const std::vector<double>& ask1,
                           const std::vector<double>& mid1,
                           const std::vector<double>& bid2,
                           const std::vector<double>& ask2,
                           const std::vector<double>& mid2){
    const size_t n = time.size();
    PriceTable out; out.reserve(n);
    for (size_t i=0; i<n; ++i){
        PriceRow r;
        r.Time = time[i];
        r.Bid1 = bid1[i]; r.Ask1 = ask1[i]; r.Mid1 = mid1[i];
        r.Bid2 = bid2[i]; r.Ask2 = ask2[i]; r.Mid2 = mid2[i];
        r.Rt   = std::log(r.Mid1 / r.Mid2);
        out.push_back(r);
    }
    return out;
}

// filtra per intervallo [start, end)
static void filter_by_date(std::vector<std::string>& t,
                           std::vector<double>& a, std::vector<double>& b, std::vector<double>& c,
                           std::vector<double>& d, std::vector<double>& e, std::vector<double>& f,
                           const std::optional<std::string>& start,
                           const std::optional<std::string>& end){
    if (!start && !end) return;

    std::vector<std::string> t2;
    std::vector<double> a2,b2,c2,d2,e2,f2;
    t2.reserve(t.size()); a2.reserve(a.size()); b2.reserve(b.size());
    c2.reserve(c.size()); d2.reserve(d.size()); e2.reserve(e.size()); f2.reserve(f.size());

    for (size_t i=0;i<t.size();++i){
        bool keep = true;
        if (start && !( *start <= t[i])) keep = false;
        if (end   && !( t[i]   < *end )) keep = false;
        if (keep){
            t2.push_back(t[i]);
            a2.push_back(a[i]); b2.push_back(b[i]); c2.push_back(c[i]);
            d2.push_back(d[i]); e2.push_back(e[i]); f2.push_back(f[i]);
        }
    }
    t.swap(t2); a.swap(a2); b.swap(b2); c.swap(c2); d.swap(d2); e.swap(e2); f.swap(f2);
}

// percentile semplice (p in [0,1])
static double percentile(std::vector<double> v, double p){
    if (v.empty()) return NAN;
    std::sort(v.begin(), v.end());
    double idx = p * (v.size()-1);
    size_t i = static_cast<size_t>(std::floor(idx));
    size_t j = static_cast<size_t>(std::ceil(idx));
    if (i==j) return v[i];
    double w = idx - i;
    return (1.0 - w)*v[i] + w*v[j];
}

// ---------------------------
// Implementazioni principali
// ---------------------------

PriceTable build_price_table(
    const std::vector<std::string>& time,
    std::optional<std::vector<double>> bid1_in,
    std::optional<std::vector<double>> ask1_in,
    std::optional<std::vector<double>> mid1_in,
    std::optional<double> tick1,
    double conv1,
    std::optional<std::vector<double>> bid2_in,
    std::optional<std::vector<double>> ask2_in,
    std::optional<std::vector<double>> mid2_in,
    std::optional<double> tick2,
    double conv2,
    const std::optional<std::string>& start_date,
    const std::optional<std::string>& end_date
){
    if (!mid1_in && (!bid1_in || !ask1_in))
        throw std::invalid_argument("Product 1: provide mid1 or both bid1/ask1.");
    if (!mid2_in && (!bid2_in || !ask2_in))
        throw std::invalid_argument("Product 2: provide mid2 or both bid2/ask2.");

    // Applica conversioni
    auto bid1 = to_conv(bid1_in, conv1);
    auto ask1 = to_conv(ask1_in, conv1);
    auto mid1 = to_conv(mid1_in, conv1);

    auto bid2 = to_conv(bid2_in, conv2);
    auto ask2 = to_conv(ask2_in, conv2);
    auto mid2 = to_conv(mid2_in, conv2);

    const size_t n = time.size();
    auto ensure_size = [&](std::vector<double>& v){
        if (v.empty()) v.resize(n, NAN);
        if (v.size()!=n) throw std::runtime_error("Column size mismatch.");
    };
    ensure_size(bid1); ensure_size(ask1); ensure_size(mid1);
    ensure_size(bid2); ensure_size(ask2); ensure_size(mid2);

    // Se mancano bid/ask ma ho mid + tick → ricostruisco come in Python
    if (mid1_in && (!bid1_in || !ask1_in)){
        if (!tick1) throw std::invalid_argument("Product 1: tick1 required when bid/ask missing.");
        bid1 = make_from_mid_with_tick(mid1, *tick1, true);
        ask1 = make_from_mid_with_tick(mid1, *tick1, false);
    } else if (!mid1_in && (bid1_in && ask1_in)) {
        // calcolo mid dalla media
        for (size_t i=0;i<n;++i) mid1[i] = 0.5*(bid1[i]+ask1[i]);
    }

    if (mid2_in && (!bid2_in || !ask2_in)){
        if (!tick2) throw std::invalid_argument("Product 2: tick2 required when bid/ask missing.");
        bid2 = make_from_mid_with_tick(mid2, *tick2, true);
        ask2 = make_from_mid_with_tick(mid2, *tick2, false);
    } else if (!mid2_in && (bid2_in && ask2_in)) {
        for (size_t i=0;i<n;++i) mid2[i] = 0.5*(bid2[i]+ask2[i]);
    }

    // eventuale filtro per data
    std::vector<std::string> t = time;
    filter_by_date(t, bid1, ask1, mid1, bid2, ask2, mid2, start_date, end_date);

    return assemble(t, bid1, ask1, mid1, bid2, ask2, mid2);
}

std::pair<PriceTable, PriceTable> split_price_table_by_months(
    const PriceTable& data, int split_months){
    if (data.empty()) return {{},{}};
    std::string split_date = add_months_iso(data.front().Time, split_months);

    PriceTable IS, OS;
    for (const auto& r : data){
        if (iso_less(r.Time, split_date)) IS.push_back(r);
        else                              OS.push_back(r);
    }
    return {IS, OS};
}

std::pair<PriceTable, PriceTable> trim_and_split_price_table(
    const PriceTable& data,
    std::optional<double> IS_start_hour,
    std::optional<double> IS_end_hour,
    std::optional<double> OS_start_hour,
    std::optional<double> OS_end_hour,
    int split_months){
    // ordina per tempo (le ISO ordinate lessicograficamente bastano)
    PriceTable sorted = data;
    std::sort(sorted.begin(), sorted.end(), [](const PriceRow& a, const PriceRow& b){
        return a.Time < b.Time;
    });

    auto [raw_IS, raw_OS] = split_price_table_by_months(sorted, split_months);

    auto in_window = [](double h, double a, double b){ return (h >= a) && (h <= b); };

    PriceTable IS = raw_IS;
    if (IS_start_hour && IS_end_hour){
        PriceTable tmp; tmp.reserve(IS.size());
        for (const auto& r : IS){
            double h = extract_decimal_hour(r.Time);
            if (in_window(h, *IS_start_hour, *IS_end_hour)) tmp.push_back(r);
        }
        IS.swap(tmp);
    }

    PriceTable OS = raw_OS;
    if (OS_start_hour && OS_end_hour){
        PriceTable tmp; tmp.reserve(OS.size());
        for (const auto& r : OS){
            double h = extract_decimal_hour(r.Time);
            // tieni solo FUORI dalla finestra esclusa
            if (h <= *OS_start_hour || h >= *OS_end_hour) tmp.push_back(r);
        }
        OS.swap(tmp);
    }

    return {IS, OS};
}

OutlierResult filter_log_spread_outliers(const PriceTable& data){
    OutlierResult R;
    if (data.empty()) return R;

    std::vector<double> Rt; Rt.reserve(data.size());
    for (auto& r : data) Rt.push_back(r.Rt);

    double Q1 = percentile(Rt, 0.25);
    double Q3 = percentile(Rt, 0.75);
    double IQR = Q3 - Q1;
    double lo  = Q1 - 3.0 * IQR;
    double hi  = Q3 + 3.0 * IQR;

    R.is_outlier.resize(data.size(), false);
    for (size_t i=0;i<data.size();++i){
        bool out = (Rt[i] < lo) || (Rt[i] > hi);
        R.is_outlier[i] = out;
        if (out) R.outliers.push_back(data[i]);
        else     R.clean.push_back(data[i]);
    }
    return R;
}

OutlierResult filter_antipersistent_outliers(const PriceTable& data){
    OutlierResult R;
    if (data.size() < 3) { R.clean = data; R.is_outlier.assign(data.size(), false); return R; }

    // IQR per soglie
    std::vector<double> Rt; Rt.reserve(data.size());
    for (auto& r : data) Rt.push_back(r.Rt);
    double Q1 = percentile(Rt, 0.25);
    double Q3 = percentile(Rt, 0.75);
    double IQR = Q3 - Q1;

    R.is_outlier.assign(data.size(), false);
    for (size_t t=1; t+1<data.size(); ++t){
        double delta_prev = std::fabs(data[t].Rt - data[t-1].Rt);
        double delta_next = std::fabs(data[t+1].Rt - data[t].Rt);
        if (delta_prev > IQR && delta_next > 0.95 * IQR) {
            R.is_outlier[t] = true;
        }
    }
    for (size_t i=0;i<data.size();++i){
        if (R.is_outlier[i]) R.outliers.push_back(data[i]);
        else                 R.clean.push_back(data[i]);
    }
    return R;
}

OutlierResult remove_outliers(const PriceTable& data){
    // Step 1: log-spread IQR
    auto R1 = filter_log_spread_outliers(data);

    // Step 2: antipersistent sul filtrato
    auto R2 = filter_antipersistent_outliers(R1.clean);

    // Ricostruzione mask completa come in Python
    OutlierResult R;
    R.is_outlier.assign(data.size(), false);

    // mark log-spread outlier
    {
        size_t j = 0;
        for (size_t i=0;i<data.size();++i){
            // se non presente in R1.clean → era outlier
            if (j < R1.clean.size() && data[i].Time == R1.clean[j].Time &&
                data[i].Rt == R1.clean[j].Rt) {
                ++j;
            } else {
                // potrebbe essere coincidenza su Time/Rt uguali; in un progetto vero,
                // usa un ID progressivo; qui la regola pratica funziona abbastanza.
                if (R.is_outlier[i]==false) R.is_outlier[i] = true;
            }
        }
    }

    // togliamo falsi positivi segnando “antipersistent” sugli elementi rimasti in clean
    {
        // mappa rapida: per ogni elemento di R1.clean, segno se R2 lo ha marcato
        // costruisco set basato su (Time, Rt)
        auto key = [](const PriceRow& r){
            return std::pair<std::string,double>{r.Time, r.Rt};
        };
        std::vector<std::pair<std::string,double>> keys_clean;
        for (auto& r : R1.clean) keys_clean.push_back(key(r));

        std::vector<bool> anti_mask_full(data.size(), false);
        // marca “anti” per elementi che compaiono in R2.is_outlier
        for (size_t idx=0; idx<R1.clean.size(); ++idx){
            if (R2.is_outlier[idx]){
                // trova nel data l’indice corrispondente
                for (size_t i=0;i<data.size();++i){
                    if (data[i].Time == keys_clean[idx].first &&
                        data[i].Rt   == keys_clean[idx].second){
                        anti_mask_full[i] = true; break;
                    }
                }
            }
        }

        for (size_t i=0;i<data.size();++i){
            if (anti_mask_full[i]) R.is_outlier[i] = true;
        }
    }

    // costruisci clean & outliers finali
    for (size_t i=0;i<data.size();++i){
        if (R.is_outlier[i]) R.outliers.push_back(data[i]);
        else                 R.clean.push_back(data[i]);
    }
    return R;
}

} // namespace util