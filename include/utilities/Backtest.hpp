#pragma once
#include <vector>
#include <string>
#include <optional>

namespace util {

struct Trade {
    // index in OS series
    size_t entry_idx = 0;
    size_t exit_idx  = 0;

    // timestamps (as read from CSV)
    std::string entry_time;
    std::string exit_time;

    // standardized z at entry/exit (sigma-units)
    double z_entry = 0.0;
    double z_exit  = 0.0;

    // raw spread X = Rt (log Mid1/Mid2)
    double x_entry = 0.0;
    double x_exit  = 0.0;

    // leverage used (>0 long-spread, <0 short-spread)
    double f = 0.0;

    // costs paid (entry + exit)
    double costs = 0.0;

    // PnL on log-spread (already net of costs)
    double pnl = 0.0;

    // number of bars in trade
    size_t bars = 0;
};

struct BacktestConfig {
    // OU params
    double k_hat;        // speed
    double eta_hat;      // long-run mean
    double sigma_hat;    // OU volatility
    // bands (sigma-units)
    double d;            // entry (negative)
    double u;            // take-profit (positive)
    double l;            // stop-loss (negative < d)
    // leverage: if NaN => use f_opt (computed by your band optimizer). Otherwise fixed.
    double f;
    // allow symmetric mirror trades (short-spread when z >= -d)
    bool symmetric = true;
};

struct BacktestMetrics {
    size_t n_trades = 0;
    size_t winners  = 0;
    double hit_ratio = 0.0;

    double sum_pnl = 0.0;        // total log-return
    double avg_pnl = 0.0;        // per trade

    double equity_end = 0.0;     // log-equity end (start=0)
    double max_dd     = 0.0;     // max drawdown on log-equity

    // rough Sharpe (per bar): mean/std of per-bar log-returns on equity diff
    double sharpe_bar = 0.0;
};

struct BacktestResult {
    BacktestMetrics metrics;
    std::vector<Trade> trades;
    // equity path (log-equity, starts at 0)
    std::vector<double> equity_path;
    // time stamps matching equity
    std::vector<std::string> equity_time;
};

// Forward declare your table/row
struct PriceRow;
using PriceTable = std::vector<PriceRow>;

/**
 * Run out-of-sample backtest on OS data.
 * - Spread X_t = Rt (already in your loader)
 * - Standardization: z_t = (X_t - eta)/(sigma_stat), sigma_stat = sigma/√(2k)
 * - Long-spread: enter when z <= d; exit TP at z >= u or SL at z <= l
 * - Symmetric (optional): short-spread mirror side
 * - Costs: use tick-by-tick actual log costs:
 *      c_t = log(Ask1/Bid1) + log(Ask2/Bid2)
 *   We charge 0.5*c_t at entry and 0.5*c_t at exit per unit leverage |f|.
 */
BacktestResult backtest_os(
    const PriceTable& os,
    const BacktestConfig& cfg
);

} // namespace util