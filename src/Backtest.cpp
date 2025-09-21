#include "utilities/Backtest.hpp"
#include "utilities/Loaders.hpp"     // for PriceRow/PriceTable
#include <cmath>
#include <algorithm>
#include <limits>

namespace util {

static inline double safe_log_ratio(double a, double b){
    return (a>0.0 && b>0.0) ? std::log(a/b) : 0.0;
}

BacktestResult backtest_os(const PriceTable& os, const BacktestConfig& cfg)
{
    BacktestResult R;

    if (os.size() < 2) return R;

    const double sigma_stat = cfg.sigma_hat / std::sqrt(2.0 * cfg.k_hat);

    // equity path init
    R.equity_path.reserve(os.size());
    R.equity_time.reserve(os.size());
    double equity = 0.0;
    double peak   = 0.0;
    double max_dd = 0.0;

    // per-bar equity diffs for Sharpe
    std::vector<double> dlog_e(os.size(), 0.0);

    enum class State { Flat, Long, Short };
    State st = State::Flat;

    size_t i_entry = 0;
    double x_entry = 0.0;
    double z_entry = 0.0;
    double f_used  = 0.0;
    double costs_acc = 0.0;

    auto flush_equity = [&](size_t i, double dlog){
        equity += dlog;
        dlog_e[i] = dlog;
        R.equity_path.push_back(equity);
        R.equity_time.push_back(os[i].Time);
        peak = std::max(peak, equity);
        max_dd = std::min(max_dd, equity - peak);
    };

    for (size_t i=0; i<os.size(); ++i){
        const auto& r = os[i];
        const double x = r.Rt; // your spread (log Mid1 / Mid2)
        const double z = (x - cfg.eta_hat) / sigma_stat;

        // half-cost at this bar per unit leverage
        const double c_bar = safe_log_ratio(r.Ask1, r.Bid1) + safe_log_ratio(r.Ask2, r.Bid2);
        const double half_cost = 0.5 * c_bar;

        double dlog_now = 0.0; // equity change this bar (if we close)

        switch (st){
        case State::Flat:
            // long entry
            if (z <= cfg.d){
                st = State::Long;
                i_entry = i;
                x_entry = x;
                z_entry = z;
                f_used  = std::isfinite(cfg.f) ? cfg.f : 1.0; // if you pass NaN, default f=1 here
                costs_acc = std::abs(f_used) * half_cost;     // entry cost
            }
            // short entry (mirror)
            else if (cfg.symmetric && z >= -cfg.d){
                st = State::Short;
                i_entry = i;
                x_entry = x;
                z_entry = z;
                f_used  = - (std::isfinite(cfg.f) ? cfg.f : 1.0);
                costs_acc = std::abs(f_used) * half_cost;
            }
            break;

        case State::Long: {
            bool takeprofit = (z >= cfg.u);
            bool stoploss   = (z <= cfg.l);

            if (takeprofit || stoploss){
                const double gross = (x - x_entry) * f_used; // log PnL (no cost)
                const double exit_cost = std::abs(f_used) * half_cost;
                costs_acc += exit_cost;

                Trade tr;
                tr.entry_idx = i_entry;
                tr.exit_idx  = i;
                tr.entry_time= os[i_entry].Time;
                tr.exit_time = r.Time;
                tr.z_entry   = z_entry;
                tr.z_exit    = z;
                tr.x_entry   = x_entry;
                tr.x_exit    = x;
                tr.f         = f_used;
                tr.costs     = costs_acc;
                tr.pnl       = gross - costs_acc;
                tr.bars      = i - i_entry;

                R.trades.push_back(tr);

                dlog_now = tr.pnl;
                st = State::Flat;
                f_used = 0.0;
                costs_acc = 0.0;
            }
            break;
        }

        case State::Short: {
            // mirror rules: exit TP when z <= -cfg.u ; stop when z >= -cfg.l
            bool takeprofit = (z <= -cfg.u);
            bool stoploss   = (z >= -cfg.l);

            if (takeprofit || stoploss){
                const double gross = (x_entry - x) * (-f_used); // or (x - x_entry)*f_used with f_used<0
                const double exit_cost = std::abs(f_used) * half_cost;
                costs_acc += exit_cost;

                Trade tr;
                tr.entry_idx = i_entry;
                tr.exit_idx  = i;
                tr.entry_time= os[i_entry].Time;
                tr.exit_time = r.Time;
                tr.z_entry   = z_entry;
                tr.z_exit    = z;
                tr.x_entry   = x_entry;
                tr.x_exit    = x;
                tr.f         = f_used;
                tr.costs     = costs_acc;
                tr.pnl       = gross - costs_acc;
                tr.bars      = i - i_entry;

                R.trades.push_back(tr);

                dlog_now = tr.pnl;
                st = State::Flat;
                f_used = 0.0;
                costs_acc = 0.0;
            }
            break;
        }
        } // switch

        flush_equity(i, dlog_now);
    }

    // compute metrics
    R.metrics.n_trades = R.trades.size();
    size_t wins = 0;
    double sum_pnl = 0.0;
    for (const auto& t : R.trades){
        sum_pnl += t.pnl;
        if (t.pnl > 0.0) ++wins;
    }
    R.metrics.winners  = wins;
    R.metrics.hit_ratio= (R.metrics.n_trades ? (double)wins / R.metrics.n_trades : 0.0);
    R.metrics.sum_pnl  = sum_pnl;
    R.metrics.avg_pnl  = (R.metrics.n_trades ? sum_pnl / R.metrics.n_trades : 0.0);
    R.metrics.equity_end = (R.equity_path.empty() ? 0.0 : R.equity_path.back());
    R.metrics.max_dd = max_dd; // negative number (log drawdown)

    // simple per-bar Sharpe on equity diffs
    // remove zeros (most bars will be 0 when flat/holding)
    double m=0.0, s=0.0;
    size_t n=0;
    for (double v : dlog_e){ if (v!=0.0){ m += v; ++n; } }
    if (n>1){
        m /= n;
        double var=0.0;
        for (double v : dlog_e){ if (v!=0.0){ double d=v-m; var+=d*d; } }
        var /= (n-1);
        s = std::sqrt(std::max(0.0, var));
        R.metrics.sharpe_bar = (s>0.0 ? m/s : 0.0);
    }

    return R;
}

} // namespace util