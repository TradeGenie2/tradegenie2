/*
 * Kai 2006@
 *
 * Copyright (C) 2006 Kai 2006@
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PORTFOLIO_CORE_H
#define PORTFOLIO_CORE_H

#include <time.h>
#include <stdbool.h>

#define MAX_PAIRS 10
#define MAX_SYMBOL_LEN 16
#define PRICE_HISTORY_SIZE 20
#define HISTORICAL_DATA_SIZE 100

#define HISTORICAL_DATA_SIZE_5M 288   
#define HISTORICAL_DATA_SIZE_15M 192  

#define HISTORICAL_DATA_SIZE_1H 500   
#define HISTORICAL_DATA_SIZE_4H 200   
#define HISTORICAL_DATA_SIZE_1D 100   
#define MAX_PATTERN_TEXT 256


typedef enum {
    POSITION_LONG = 0,   
    POSITION_SHORT = 1   
} PositionType;


typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    double bought_price;      
    double quantity;
    double current_price;
    PositionType position_type;  
    double price_history[PRICE_HISTORY_SIZE];
    int history_count;
    int history_index;
    
    
    double historical_prices[HISTORICAL_DATA_SIZE];
    int historical_count;
    bool historical_loaded;
    time_t last_historical_fetch;
    
    
    double historical_5m[HISTORICAL_DATA_SIZE_5M];
    int historical_5m_count;
    bool historical_5m_loaded;
    time_t last_5m_fetch;
    
    double historical_15m[HISTORICAL_DATA_SIZE_15M];
    int historical_15m_count;
    bool historical_15m_loaded;
    time_t last_15m_fetch;
    
    
    double historical_1h[HISTORICAL_DATA_SIZE_1H];
    int historical_1h_count;
    bool historical_1h_loaded;
    time_t last_1h_fetch;
    
    double historical_4h[HISTORICAL_DATA_SIZE_4H];
    int historical_4h_count;
    bool historical_4h_loaded;
    time_t last_4h_fetch;
    
    double historical_1d[HISTORICAL_DATA_SIZE_1D];
    int historical_1d_count;
    bool historical_1d_loaded;
    time_t last_1d_fetch;
    
    
    double ema_12, ema_26, ema_50, ema_200;
    double macd, macd_signal;
    double bb_upper, bb_middle, bb_lower;
    
    
    double scalp_trend;        
    double scalp_momentum;     
    char scalp_signal[64];     
    
    
    double profit_probability;  
    char detected_patterns[MAX_PATTERN_TEXT];
    int pattern_count;
} TradingPair;

typedef struct {
    TradingPair pairs[MAX_PAIRS];
    int pair_count;
} Portfolio;

typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    double profit_loss_percent;
    double profit_loss_value;
    double current_value;
    double current_price;
    double bought_price;
    double quantity;
    PositionType position_type;  
    int trend;
    double momentum_score;
} PerformanceItem;

typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    double current_price;
    double price_change_24h;
    double volume_24h;
    int trend;
    double momentum;
    double rsi;
    double volatility;
    int score;
    double suggested_buy_price;
    double suggested_sell_price;
} InvestmentOpportunity;


typedef struct {
    double target_price;          
    double probability;           
    double expected_profit_loss;  
    double expected_profit_pct;   
    int estimated_hours;          
    char confidence_level[32];    
    char reasoning[128];          
} TargetAnalysis;


Portfolio* portfolio_create(void);
void portfolio_destroy(Portfolio *portfolio);
void portfolio_init_default(Portfolio *portfolio);
bool portfolio_load(Portfolio *portfolio);
void portfolio_save(Portfolio *portfolio);
char* portfolio_get_file_path(void);


int portfolio_add_pair(Portfolio *portfolio, const char *symbol, double bought_price, 
                       double quantity, PositionType position_type);
void portfolio_remove_pair(Portfolio *portfolio, int index);
void portfolio_update_pair(Portfolio *portfolio, int index, const char *symbol, 
                          double bought_price, double quantity, PositionType position_type);
void portfolio_update_current_price(Portfolio *portfolio, int index, double price);


int portfolio_calculate_trend(const TradingPair *pair);
double portfolio_calculate_momentum(const TradingPair *pair);
double portfolio_calculate_volatility(const TradingPair *pair);
double portfolio_calculate_rsi(const TradingPair *pair);
void portfolio_calculate_support_resistance(const TradingPair *pair, double *support, double *resistance);
void portfolio_calculate_trade_prices(const TradingPair *pair, double *buy_price, double *sell_price, 
                                       char *buy_reason, char *sell_reason);


void portfolio_analyze_target(const TradingPair *pair, double target_price, 
                              bool is_sell_target, TargetAnalysis *analysis);
double calculate_target_probability(const TradingPair *pair, double target_price, 
                                    double current_price, int trend, double volatility);
int estimate_time_to_target(const TradingPair *pair, double target_price, 
                           double current_price, double volatility);


double calculate_ema(const double *prices, int count, int period);
void calculate_macd(const TradingPair *pair, double *macd, double *signal, double *histogram);
void calculate_bollinger_bands(const TradingPair *pair, double *upper, double *middle, double *lower);
int detect_ema_cross(const TradingPair *pair, int fast_period, int slow_period);


bool detect_double_bottom(const double *prices, int count, int *pattern_idx);
bool detect_double_top(const double *prices, int count, int *pattern_idx);
bool detect_head_shoulders(const double *prices, int count, int *pattern_idx);
bool detect_inverse_head_shoulders(const double *prices, int count, int *pattern_idx);


int calculate_trend_multi_timeframe(const TradingPair *pair);
double calculate_profit_probability(const TradingPair *pair);
void update_all_indicators(TradingPair *pair);


void analyze_scalping_signals(TradingPair *pair);
int get_scalping_trend(const TradingPair *pair);
double get_scalping_momentum(const TradingPair *pair);
const char* get_scalping_signal(const TradingPair *pair);
bool is_scalp_buy_signal(const TradingPair *pair);
bool is_scalp_sell_signal(const TradingPair *pair);


void portfolio_analyze_performance(Portfolio *portfolio, PerformanceItem *items, int *item_count);
const char* portfolio_get_recommendation_text(const TradingPair *pair, double profit_percent, 
                                               int trend, double momentum);
const char* portfolio_get_recommendation_color(const TradingPair *pair, double profit_percent, 
                                                int trend, double momentum);


double portfolio_get_total_value(const Portfolio *portfolio);
double portfolio_get_total_cost(const Portfolio *portfolio);
double portfolio_get_total_profit_loss(const Portfolio *portfolio);
double portfolio_get_total_profit_loss_percent(const Portfolio *portfolio);

#endif 
