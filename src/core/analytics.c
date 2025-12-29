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

#include "portfolio/portfolio_core.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int portfolio_calculate_trend(const TradingPair *pair) {
    if (!pair || pair->history_count < 2) {
        return 0; 
    }
    
    int recent_count = (pair->history_count < 5) ? pair->history_count : 5;
    double sum = 0.0;
    int start_idx = (pair->history_index - recent_count + PRICE_HISTORY_SIZE) % PRICE_HISTORY_SIZE;
    
    for (int i = 0; i < recent_count; i++) {
        int idx = (start_idx + i) % PRICE_HISTORY_SIZE;
        sum += pair->price_history[idx];
    }
    double recent_avg = sum / recent_count;
    
    if (pair->current_price > recent_avg * 1.02) {
        return 1; 
    } else if (pair->current_price < recent_avg * 0.98) {
        return -1; 
    }
    return 0; 
}

double portfolio_calculate_momentum(const TradingPair *pair) {
    if (!pair || pair->history_count < 10) {
        return 0.0;
    }
    
    int count = (pair->history_count < 10) ? pair->history_count : 10;
    double sum_numerator = 0.0;
    double sum_denominator = 0.0;
    
    int start_idx = (pair->history_index - count + PRICE_HISTORY_SIZE) % PRICE_HISTORY_SIZE;
    
    for (int i = 1; i < count; i++) {
        int idx = (start_idx + i) % PRICE_HISTORY_SIZE;
        int prev_idx = (start_idx + i - 1) % PRICE_HISTORY_SIZE;
        
        double change = pair->price_history[idx] - pair->price_history[prev_idx];
        double weight = (double)i / count;
        
        sum_numerator += change * weight;
        sum_denominator += weight;
    }
    
    return (sum_denominator > 0) ? (sum_numerator / sum_denominator) : 0.0;
}

double portfolio_calculate_volatility(const TradingPair *pair) {
    if (!pair) return 0.0;
    
    int count = 0;
    double sum = 0.0;
    
    if (pair->historical_loaded && pair->historical_count > 0) {
        count = pair->historical_count;
        for (int i = 0; i < count; i++) {
            sum += pair->historical_prices[i];
        }
    } else if (pair->history_count > 0) {
        count = pair->history_count;
        for (int i = 0; i < count; i++) {
            sum += pair->price_history[i];
        }
    } else {
        return 0.0;
    }
    
    if (count == 0) return 0.0;
    
    double mean = sum / count;
    double variance = 0.0;
    
    if (pair->historical_loaded && pair->historical_count > 0) {
        for (int i = 0; i < pair->historical_count; i++) {
            double diff = pair->historical_prices[i] - mean;
            variance += diff * diff;
        }
    } else {
        for (int i = 0; i < pair->history_count; i++) {
            double diff = pair->price_history[i] - mean;
            variance += diff * diff;
        }
    }
    
    variance /= count;
    double std_dev = sqrt(variance);
    
    return (mean > 0) ? (std_dev / mean) : 0.0;
}

double portfolio_calculate_rsi(const TradingPair *pair) {
    if (!pair) return 50.0;
    
    int period = 14;
    int count = 0;
    const double *prices = NULL;
    
    if (pair->historical_loaded && pair->historical_count >= period) {
        count = pair->historical_count;
        prices = pair->historical_prices;
    } else if (pair->history_count >= period) {
        count = pair->history_count;
        prices = pair->price_history;
    } else {
        return 50.0;
    }
    
    double gains = 0.0;
    double losses = 0.0;
    
    int start_idx = count - period;
    for (int i = start_idx; i < count - 1; i++) {
        double change = prices[i + 1] - prices[i];
        if (change > 0) {
            gains += change;
        } else {
            losses += fabs(change);
        }
    }
    
    double avg_gain = gains / period;
    double avg_loss = losses / period;
    
    if (avg_loss == 0) {
        return 100.0;
    }
    
    double rs = avg_gain / avg_loss;
    double rsi = 100.0 - (100.0 / (1.0 + rs));
    
    return rsi;
}

void portfolio_calculate_support_resistance(const TradingPair *pair, double *support, double *resistance) {
    if (!pair || !support || !resistance) return;
    
    *support = 0.0;
    *resistance = 0.0;
    
    int count = 0;
    const double *prices = NULL;
    
    if (pair->historical_loaded && pair->historical_count > 0) {
        count = pair->historical_count;
        prices = pair->historical_prices;
    } else if (pair->history_count > 0) {
        count = pair->history_count;
        prices = pair->price_history;
    } else {
        return;
    }
    
    if (count < 3) return;
    
    double min_price = prices[0];
    double max_price = prices[0];
    
    for (int i = 1; i < count; i++) {
        if (prices[i] < min_price) min_price = prices[i];
        if (prices[i] > max_price) max_price = prices[i];
    }
    
    *support = min_price;
    *resistance = max_price;
}

void portfolio_calculate_trade_prices(const TradingPair *pair, double *buy_price, double *sell_price,
                                       char *buy_reason, char *sell_reason) {
    if (!pair || !buy_price || !sell_price) return;
    
    double support = 0.0, resistance = 0.0;
    portfolio_calculate_support_resistance(pair, &support, &resistance);
    
    if (buy_reason) buy_reason[0] = '\0';
    if (sell_reason) sell_reason[0] = '\0';
    
    double rsi = portfolio_calculate_rsi(pair);
    double volatility = portfolio_calculate_volatility(pair);
    int trend = portfolio_calculate_trend(pair);
    
    double current = pair->current_price > 0 ? pair->current_price : pair->bought_price;
    if (current <= 0.0) {
        double fallback = (pair->position_type == POSITION_SHORT) ? resistance : support;
        current = (fallback > 0.0) ? fallback : 1.0;
    }
    
    
    if (pair->position_type == POSITION_SHORT) {
        
        if (trend > 0 && rsi > 60) {
            *buy_price = current * 1.02;
            if (buy_reason) strcpy(buy_reason, "[!] COVER - Uptrend forming");
        } else if (rsi < 30) {
            *buy_price = current * 0.97;
            if (buy_reason) strcpy(buy_reason, "[+] COVER - Target reached");
        } else if (support > 0 && current > support * 1.01) {
            *buy_price = support * 1.01;
            if (buy_reason) strcpy(buy_reason, "[o] COVER - Near support");
        } else {
            *buy_price = current * 0.98;
            if (buy_reason) strcpy(buy_reason, "[$] COVER - Take profit");
        }
        
        
        if (rsi > 70 && trend <= 0) {
            *sell_price = current * 1.03;
            if (sell_reason) strcpy(sell_reason, "[+] ADD SHORT - Overbought");
        } else if (resistance > 0 && current < resistance * 0.98) {
            *sell_price = resistance * 0.99;
            if (sell_reason) strcpy(sell_reason, "[+] ADD SHORT - At resistance");
        } else if (trend < 0) {
            double target = (volatility > 0.1) ? 1.04 : 1.02;
            *sell_price = current * target;
            if (sell_reason) strcpy(sell_reason, "[+] ADD SHORT - Downtrend");
        } else {
            *sell_price = current * 1.02;
            if (sell_reason) strcpy(sell_reason, "[+] ADD SHORT - On rally");
        }
        return;
    }
    
    
    
    if (rsi < 30 && trend >= 0) {
        *buy_price = current * 0.97;
        if (buy_reason) strcpy(buy_reason, "➕ ADD LONG - Oversold bounce");
    } else if (support > 0 && current > support * 1.02) {
        *buy_price = support * 1.01;
        if (buy_reason) strcpy(buy_reason, "➕ ADD LONG - At support");
    } else if (trend > 0 && rsi < 50) {
        *buy_price = current * 0.98;
        if (buy_reason) strcpy(buy_reason, "[+] ADD LONG - Uptrend dip");
    } else {
        *buy_price = current * 0.97;
        if (buy_reason) strcpy(buy_reason, "[+] ADD LONG - Below current");
    }
    
    
    if (rsi > 70 && trend <= 0) {
        *sell_price = current * 1.03;
        if (sell_reason) strcpy(sell_reason, "[$] SELL - Overbought peak");
    } else if (resistance > 0 && current < resistance * 0.98) {
        *sell_price = resistance * 0.99;
        if (sell_reason) strcpy(sell_reason, "[o] SELL - At resistance");
    } else if (trend > 0) {
        double target = (volatility > 0.1) ? 1.05 : 1.03;
        *sell_price = current * target;
        if (sell_reason) strcpy(sell_reason, "[+] SELL - Target profit");
    } else {
        *sell_price = current * 1.02;
        if (sell_reason) strcpy(sell_reason, "[!] SELL - Protect gains");
    }
}

const char* portfolio_get_recommendation_text(const TradingPair *pair, double profit_percent,
                                               int trend, double momentum) {
    if (!pair) return "HOLD";
    
    double rsi = portfolio_calculate_rsi(pair);
    bool is_long = (pair->position_type == POSITION_LONG);
    
    
    if (is_long) {
        
        if (profit_percent > 15 && rsi > 70) {
            return "🎯 TAKE PROFIT - Overbought";
        } else if (profit_percent > 10 && (rsi > 65 || trend < 0)) {
            return "💰 TAKE PROFIT - Good gain";
        
        } else if (profit_percent > 5 && trend > 0 && rsi < 65) {
            return "✅ HOLD - Uptrend continues";
        
        } else if (profit_percent < -15 && rsi < 25) {
            return "🔻 OVERSOLD - Consider averaging";
        } else if (profit_percent < -10 && trend < 0) {
            return "⚠️ STOP LOSS - Downtrend";
        } else if (profit_percent < -5 && rsi < 35 && trend >= 0) {
            return "📊 ACCUMULATE - RSI low, trend ok";
        
        } else if (trend > 0 && rsi < 60) {
            return "📈 HOLD - Bullish momentum";
        } else if (trend < 0 && rsi > 50) {
            return "⚡ CAUTION - Bearish pressure";
        }
        return "⏸️ HOLD - Monitor closely";
    } 
    
    else {
        
        if (profit_percent > 15 && rsi < 30) {
            return "🎯 COVER SHORT - Oversold";
        } else if (profit_percent > 10 && (rsi < 35 || trend > 0)) {
            return "💰 COVER SHORT - Good profit";
        
        } else if (profit_percent > 5 && trend < 0 && rsi > 35) {
            return "✅ HOLD SHORT - Downtrend continues";
        
        } else if (profit_percent < -15 && rsi > 75) {
            return "🔻 OVERBOUGHT - Add to short";
        } else if (profit_percent < -10 && trend > 0) {
            return "⚠️ COVER SHORT - Uptrend against you";
        } else if (profit_percent < -5 && rsi > 65 && trend <= 0) {
            return "📊 ADD SHORT - RSI high, trend ok";
        
        } else if (trend < 0 && rsi > 40) {
            return "📉 HOLD SHORT - Bearish momentum";
        } else if (trend > 0 && rsi < 50) {
            return "⚡ CAUTION - Bullish pressure";
        }
        return "⏸️ HOLD SHORT - Monitor closely";
    }
}

const char* portfolio_get_recommendation_color(const TradingPair *pair, double profit_percent,
                                                 int trend, double momentum) {
    if (!pair) return "#FFB800";
    
    double rsi = portfolio_calculate_rsi(pair);
    bool is_long = (pair->position_type == POSITION_LONG);
    
    
    if (is_long) {
        
        if (profit_percent > 10 && (rsi > 65 || trend < 0)) {
            return "#30d158";  
        
        } else if (profit_percent > 5 && trend > 0) {
            return "#30B0C7";  
        
        } else if (profit_percent < -10 && rsi < 30 && trend >= 0) {
            return "#64d2ff";  
        
        } else if (profit_percent < -10 && trend < 0) {
            return "#ff453a";  
        
        } else if (profit_percent < -5 || (trend < 0 && rsi > 50)) {
            return "#ff9f0a";  
        
        } else {
            return "#98989d";  
        }
    }
    
    else {
        
        if (profit_percent > 10 && (rsi < 35 || trend > 0)) {
            return "#30d158";  
        
        } else if (profit_percent > 5 && trend < 0) {
            return "#30B0C7";  
        
        } else if (profit_percent < -10 && rsi > 70 && trend <= 0) {
            return "#64d2ff";  
        
        } else if (profit_percent < -10 && trend > 0) {
            return "#ff453a";  
        
        } else if (profit_percent < -5 || (trend > 0 && rsi < 50)) {
            return "#ff9f0a";  
        
        } else {
            return "#98989d";  
        }
    }
}

void portfolio_analyze_performance(Portfolio *portfolio, PerformanceItem *items, int *item_count) {
    if (!portfolio || !items || !item_count) return;
    
    *item_count = portfolio->pair_count;
    
    for (int i = 0; i < portfolio->pair_count; i++) {
        TradingPair *pair = &portfolio->pairs[i];
        strncpy(items[i].symbol, pair->symbol, MAX_SYMBOL_LEN - 1);
        items[i].symbol[MAX_SYMBOL_LEN - 1] = '\0';
        
        items[i].bought_price = pair->bought_price;
        items[i].quantity = pair->quantity;
        items[i].current_price = pair->current_price;
        items[i].position_type = pair->position_type;
        
        double cost = pair->bought_price * pair->quantity;
        
        if (pair->position_type == POSITION_LONG) {
            
            items[i].current_value = pair->current_price * pair->quantity;
            items[i].profit_loss_value = items[i].current_value - cost;
        } else {
            
            double current_value = pair->current_price * pair->quantity;
            items[i].profit_loss_value = cost - current_value;
            items[i].current_value = cost + items[i].profit_loss_value;
        }
        
        items[i].profit_loss_percent = (cost > 0) ? 
            (items[i].profit_loss_value / cost) * 100.0 : 0.0;
        
        items[i].trend = portfolio_calculate_trend(pair);
        items[i].momentum_score = portfolio_calculate_momentum(pair);
    }
}


double calculate_target_probability(const TradingPair *pair, double target_price, 
                                    double current_price, int trend, double volatility) {
    if (!pair || current_price <= 0 || target_price <= 0) return 0.5;
    
    double price_diff_pct = fabs((target_price - current_price) / current_price);
    double rsi = portfolio_calculate_rsi(pair);
    
    
    double probability = 0.5;
    
    
    bool target_is_higher = (target_price > current_price);
    if ((target_is_higher && trend > 0) || (!target_is_higher && trend < 0)) {
        probability += 0.15;  
    } else if ((target_is_higher && trend < 0) || (!target_is_higher && trend > 0)) {
        probability -= 0.15;  
    }
    
    
    if (target_is_higher) {
        
        if (rsi < 40) probability += 0.10;       
        else if (rsi > 70) probability -= 0.15;  
    } else {
        
        if (rsi > 60) probability += 0.10;       
        else if (rsi < 30) probability -= 0.15;  
    }
    
    
    if (price_diff_pct < 0.02) {        
        probability += 0.20;  
    } else if (price_diff_pct < 0.05) { 
        probability += 0.10;
    } else if (price_diff_pct > 0.10) { 
        probability -= 0.15;  
    }
    
    
    if (volatility > 0.08) {       
        if (price_diff_pct < 0.05) {
            probability += 0.05;   
        }
    } else if (volatility < 0.02) { 
        if (price_diff_pct > 0.05) {
            probability -= 0.10;   
        }
    }
    
    
    if (probability < 0.15) probability = 0.15;
    if (probability > 0.95) probability = 0.95;
    
    return probability;
}


int estimate_time_to_target(const TradingPair *pair, double target_price, 
                           double current_price, double volatility) {
    if (!pair || current_price <= 0 || target_price <= 0) return 0;
    
    double price_diff_pct = fabs((target_price - current_price) / current_price);
    
    
    double avg_daily_movement = 0.03;  
    
    if (pair->historical_1d_loaded && pair->historical_1d_count > 5) {
        
        double total_movement = 0.0;
        int movement_count = 0;
        for (int i = 1; i < pair->historical_1d_count && i < 20; i++) {
            double pct_change = fabs((pair->historical_1d[i] - pair->historical_1d[i-1]) / pair->historical_1d[i-1]);
            total_movement += pct_change;
            movement_count++;
        }
        if (movement_count > 0) {
            avg_daily_movement = total_movement / movement_count;
        }
    } else if (volatility > 0) {
        
        avg_daily_movement = volatility * 1.5;  
    }
    
    
    if (avg_daily_movement < 0.01) avg_daily_movement = 0.01;
    if (avg_daily_movement > 0.15) avg_daily_movement = 0.15;  
    
    
    double estimated_days = price_diff_pct / avg_daily_movement;
    
    
    if (price_diff_pct > 0.05) {
        estimated_days *= 1.3;  
    }
    
    
    int hours = (int)(estimated_days * 24);
    
    
    if (hours < 1) hours = 1;
    if (hours > 720) hours = 720;
    
    return hours;
}


void portfolio_analyze_target(const TradingPair *pair, double target_price, 
                              bool is_sell_target, TargetAnalysis *analysis) {
    if (!pair || !analysis || target_price <= 0) return;
    
    double current_price = pair->current_price > 0 ? pair->current_price : pair->bought_price;
    int trend = portfolio_calculate_trend(pair);
    double volatility = portfolio_calculate_volatility(pair);
    double rsi = portfolio_calculate_rsi(pair);
    
    
    analysis->target_price = target_price;
    
    
    analysis->probability = calculate_target_probability(pair, target_price, 
                                                         current_price, trend, volatility);
    
    
    if (is_sell_target) {
        
        if (pair->position_type == POSITION_LONG) {
            
            analysis->expected_profit_loss = (target_price - pair->bought_price) * pair->quantity;
            analysis->expected_profit_pct = ((target_price - pair->bought_price) / pair->bought_price) * 100.0;
        } else {
            
            analysis->expected_profit_loss = (pair->bought_price - target_price) * pair->quantity;
            analysis->expected_profit_pct = ((pair->bought_price - target_price) / pair->bought_price) * 100.0;
        }
    } else {
        
        if (pair->position_type == POSITION_LONG) {
            
            double avg_price = (pair->bought_price + target_price) / 2.0;
            analysis->expected_profit_loss = (current_price - avg_price) * pair->quantity;
            analysis->expected_profit_pct = ((target_price - current_price) / current_price) * 100.0;
        } else {
            
            analysis->expected_profit_loss = (pair->bought_price - target_price) * pair->quantity;
            analysis->expected_profit_pct = ((pair->bought_price - target_price) / pair->bought_price) * 100.0;
        }
    }
    
    
    analysis->estimated_hours = estimate_time_to_target(pair, target_price, 
                                                        current_price, volatility);
    
    
    if (analysis->probability >= 0.75) {
        strcpy(analysis->confidence_level, "High");
    } else if (analysis->probability >= 0.55) {
        strcpy(analysis->confidence_level, "Medium");
    } else {
        strcpy(analysis->confidence_level, "Low");
    }
    
    
    char trend_str[16] = "neutral";
    if (trend > 0) strcpy(trend_str, "uptrend");
    else if (trend < 0) strcpy(trend_str, "downtrend");
    
    char rsi_str[32] = "neutral";
    if (rsi > 70) strcpy(rsi_str, "overbought");
    else if (rsi < 30) strcpy(rsi_str, "oversold");
    
    double price_diff_pct = fabs((target_price - current_price) / current_price) * 100.0;
    
    snprintf(analysis->reasoning, sizeof(analysis->reasoning),
             "%s, %s, %.1f%% away, vol %.1f%%",
             trend_str, rsi_str, price_diff_pct, volatility * 100.0);
}
