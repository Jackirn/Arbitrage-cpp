#include "utilities/Loaders.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace util {

// --------------------- helpers base ---------------------
static bool is_space_like(unsigned char c){
    // NBSP (0xA0) + spazi standard
    return std::isspace(c) || c == 0xA0;
}

static std::string trim_spaces(std::string s){
    size_t i=0, j=s.size();
    while (i<j && is_space_like((unsigned char)s[i])) ++i;
    while (j>i && is_space_like((unsigned char)s[j-1])) --j;
    return s.substr(i, j-i);
}

static std::vector<std::string> split_with_delim(const std::string& line, char delim){
    std::vector<std::string> out; out.reserve(64);
    std::string cur; bool in_quotes=false;
    for(char ch: line){
        if (ch=='"'){ in_quotes=!in_quotes; continue; }
        if (ch==delim && !in_quotes){ out.push_back(trim_spaces(cur)); cur.clear(); }
        else cur.push_back(ch);
    }
    out.push_back(trim_spaces(cur));
    return out;
}

static std::vector<std::string> split_auto(const std::string& line){
    auto a = split_with_delim(line, ',');
    auto b = split_with_delim(line, ';');
    if (b.size()>a.size() && b.size()>1) return b;
    if (a.size()>1) return a;
    if (b.size()>1) return b;
    return a;
}

static void to_upper_inplace(std::string& s){
    for(char& c: s) c = (char)std::toupper((unsigned char)c);
}

static void replace_nan_na_with_empty(std::vector<std::string>& row){
    for (auto& s : row){
        std::string t = s;
        to_upper_inplace(t);
        if (t=="NAN" || t=="NA") s.clear();
    }
}

static void ffill_inplace(std::vector<std::string>& row){
    for (size_t i=0;i<row.size();++i){
        if (!row[i].empty()) continue;
        if (i>0 && !row[i-1].empty()) row[i] = row[i-1];
    }
}

static size_t need_index(const std::vector<std::string>& headers, const std::string& name){
    for (size_t i=0;i<headers.size();++i)
        if (headers[i] == name) return i;
    throw std::runtime_error("Missing column: " + name);
}

static bool to_double(std::string s, double& out){
    for (char& ch : s) if (ch==',') ch='.';           // virgola -> punto
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){
        return c==' ' || c=='\t' || c==0xA0;
    }), s.end());
    try { size_t idx=0; out = std::stod(s, &idx); return idx>0; }
    catch(...) { return false; }
}

// --------------------- date helpers ---------------------
static bool looks_like_iso(const std::string& s){
    if (s.size() < 10) return false;
    return std::isdigit((unsigned char)s[0]) &&
           std::isdigit((unsigned char)s[1]) &&
           std::isdigit((unsigned char)s[2]) &&
           std::isdigit((unsigned char)s[3]) &&
           s[4]=='-' && s[7]=='-';
}

static std::string pad2(int x){ char buf[3]; std::snprintf(buf, sizeof(buf), "%02d", x); return buf; }
static std::string pad4(int x){ char buf[5]; std::snprintf(buf, sizeof(buf), "%04d", x); return buf; }

static int yy_to_yyyy(int yy){ return (yy <= 69) ? (2000 + yy) : (1900 + yy); }

static std::string to_iso_datetime_eu(std::string s){
    s = trim_spaces(s);
    if (s.empty()) return s;

    if (looks_like_iso(s)) {
        if (s.size()==10) return s + " 00:00:00";
        if (s.size()>=16 && s.find(':', 14) != std::string::npos){
            size_t first_col = s.find(':', 11);
            size_t second_col = (first_col==std::string::npos)? std::string::npos : s.find(':', first_col+1);
            if (second_col==std::string::npos) return s + ":00";
        }
        return s;
    }

    std::string date, time;
    {
        size_t sp = s.find(' ');
        if (sp == std::string::npos) { date = s; time = "00:00:00"; }
        else { date = s.substr(0, sp); time = trim_spaces(s.substr(sp+1)); }
    }

    int d=0,m=0,y=0;
    {
        std::vector<int> parts; parts.reserve(3);
        std::string cur;
        for(char c: date){
            if (c=='/' || c=='-' || c=='.'){ if(!cur.empty()){ parts.push_back(std::stoi(cur)); cur.clear(); } }
            else if (std::isdigit((unsigned char)c)) cur.push_back(c);
        }
        if(!cur.empty()) parts.push_back(std::stoi(cur));
        if (parts.size()<3) return s;
        d = parts[0]; m = parts[1]; y = parts[2];
        if (y < 100) y = yy_to_yyyy(y);
    }

    int H=0, M=0, S=0;
    if (!time.empty()){
        std::vector<int> t; t.reserve(3);
        std::string cur;
        for(char c: time){
            if (c==':' || c==' '){ if(!cur.empty()){ t.push_back(std::stoi(cur)); cur.clear(); } }
            else if (std::isdigit((unsigned char)c)) cur.push_back(c);
        }
        if(!cur.empty()) t.push_back(std::stoi(cur));
        if (!t.empty()) H = t[0];
        if (t.size()>=2) M = t[1];
        if (t.size()>=3) S = t[2];
    }

    return pad4(y) + "-" + pad2(m) + "-" + pad2(d) + " " + pad2(H) + ":" + pad2(M) + ":" + pad2(S);
}

// --------------------- funzione principale ---------------------
PriceTable load_and_process_price_data_csv(
    const std::string& filepath,
    const std::string& time_col,
    const std::array<std::string,4>& bid_ask_cols,
    const std::optional<std::array<std::string,2>>& mid_cols,
    const std::optional<std::array<double,2>>& ticks,
    const std::array<double,2>& convs,
    const std::optional<std::string>& start_date,
    const std::optional<std::string>& end_date
){
    std::ifstream fin(filepath);
    if (!fin.is_open()) throw std::runtime_error("Cannot open CSV: " + filepath);

    auto read_header_row = [&](std::vector<std::string>& out)->bool {
        std::string line;
        while (std::getline(fin, line)) {
            if (line.empty()) continue;
            auto cols = split_auto(line);
            size_t nonempty = 0;
            for (auto& c : cols) if (!c.empty()) ++nonempty;
            if (cols.size() >= 4 && nonempty >= 2) { out = std::move(cols); return true; }
        }
        return false;
    };

    std::vector<std::string> raw1, raw2;
    if (!read_header_row(raw1)) throw std::runtime_error("Empty CSV: " + filepath);
    if (!read_header_row(raw2)) throw std::runtime_error("CSV senza seconda riga d'intestazione: " + filepath);

    replace_nan_na_with_empty(raw1);
    replace_nan_na_with_empty(raw2);
    ffill_inplace(raw1);

    std::vector<std::string> headers;
    const size_t W = std::max(raw1.size(), raw2.size());
    headers.reserve(W);
    for (size_t i=0; i<W; ++i){
        std::string a = i<raw1.size()? trim_spaces(raw1[i]) : "";
        std::string b = i<raw2.size()? trim_spaces(raw2[i]) : "";
        if (!a.empty() && !b.empty()) headers.push_back(a + "_" + b);
        else if (!b.empty())           headers.push_back(b);
        else                           headers.push_back(a);
    }

    std::cerr << "=== Debug CSV Headers (combined) ===\n";
    for (auto& h : headers) std::cerr << "  [" << h << "]\n";

    std::string time_name = time_col;
    if (time_col == "*"){
        for (auto& h : headers) {
            if (h == "Timestamp") { time_name = h; break; }
        }
        if (time_name == "*"){
            for (auto& h : headers){
                if (h.find("Timestamp") != std::string::npos){
                    time_name = h; break;
                }
            }
        }
        if (time_name == "*"){
            for (size_t i=0; i<raw2.size(); ++i){
                if (trim_spaces(raw2[i]) == "Timestamp"){
                    time_name = headers[i];
                    break;
                }
            }
        }
        if (time_name == "*"){
            for (auto& h : headers){
                if (!h.empty()){ time_name = h; break; }
            }
        }
        if (time_name == "*")
            throw std::runtime_error("Auto time_col failed: no Timestamp-like column found");
        std::cerr << "Auto-detected time column: [" << time_name << "]\n";
    }

    const size_t tcol = need_index(headers, time_name);
    const size_t b1   = need_index(headers, bid_ask_cols[0]);
    const size_t a1   = need_index(headers, bid_ask_cols[1]);
    const size_t b2   = need_index(headers, bid_ask_cols[2]);
    const size_t a2   = need_index(headers, bid_ask_cols[3]);
    size_t m1 = (size_t)-1, m2 = (size_t)-1;
    if (mid_cols){
        m1 = need_index(headers, (*mid_cols)[0]);
        m2 = need_index(headers, (*mid_cols)[1]);
    }

    // Prepara i bound ISO [start, end)
    std::string start_iso, end_iso;
    if (start_date) start_iso = to_iso_datetime_eu(*start_date);
    if (end_date)   end_iso   = to_iso_datetime_eu(*end_date);

    PriceTable out;
    std::string line;
    while (std::getline(fin, line)){
        if (line.empty()) continue;
        auto cols = split_auto(line);
        if (cols.size() <= std::max({tcol,b1,a1,b2,a2})) continue;

        PriceRow r;
        std::string rawT = trim_spaces(cols[tcol]);
        r.Time = to_iso_datetime_eu(rawT);

        if (!start_iso.empty() && r.Time < start_iso) continue;
        if (!end_iso.empty()   && r.Time >= end_iso) continue;

        double B1=0,A1=0,M1=0,B2=0,A2=0,M2=0;
        to_double(cols[b1], B1);
        to_double(cols[a1], A1);
        to_double(cols[b2], B2);
        to_double(cols[a2], A2);
        if (mid_cols){
            to_double(cols[m1], M1);
            to_double(cols[m2], M2);
        }

        if (M1==0.0 && B1!=0.0 && A1!=0.0) M1 = 0.5*(B1+A1);
        if (M2==0.0 && B2!=0.0 && A2!=0.0) M2 = 0.5*(B2+A2);
        if ((B1==0.0 || A1==0.0) && M1!=0.0 && ticks){ B1 = M1 - (*ticks)[0]/2.0; A1 = M1 + (*ticks)[0]/2.0; }
        if ((B2==0.0 || A2==0.0) && M2!=0.0 && ticks){ B2 = M2 - (*ticks)[1]/2.0; A2 = M2 + (*ticks)[1]/2.0; }

        r.Bid1 = B1 * convs[0]; r.Ask1 = A1 * convs[0]; r.Mid1 = M1 * convs[0];
        r.Bid2 = B2 * convs[1]; r.Ask2 = A2 * convs[1]; r.Mid2 = M2 * convs[1];

        if (r.Mid1>0 && r.Mid2>0) r.Rt = std::log(r.Mid1 / r.Mid2);

        out.push_back(r);
    }

    return out;
}

} // namespace util