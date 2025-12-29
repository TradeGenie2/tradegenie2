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


double calculate_ema(const double *prices, int count, int period) {
    if (!prices || count < period || period < 1) {
        return 0.0;
    }
    
    
    double sum = 0.0;
    for (int i = 0; i < period; i++) {
        sum += prices[i];
    }
    double ema = sum / period;
    
    
    double multiplier = 2.0 / (period + 1.0);
    
    
    for (int i = period; i < count; i++) {
        ema = (prices[i] - ema) * multiplier + ema;
    }
    
    return ema;
}


void calculate_macd(const TradingPair *pair, double *macd, double *signal, double *histogram) {
    if (!pair || !macd || !signal || !histogram) {
        return;
    }
    
    *macd = 0.0;
    *signal = 0.0;
    *histogram = 0.0;
    
    const double *prices = NULL;
    int count = 0;
    
    
    if (pair->historical_1h_loaded && pair->historical_1h_count >= 26) {
        prices = pair->historical_1h;
        count = pair->historical_1h_count;
    } else if (pair->historical_loaded && pair->historical_count >= 26) {
        prices = pair->historical_prices;
        count = pair->historical_count;
    } else {
        return;
    }
    
    
    double ema12 = calculate_ema(prices, count, 12);
    double ema26 = calculate_ema(prices, count, 26);
    
    
    *macd = ema12 - ema26;
    
    
    *signal = *macd * 0.9;  
    
    
    *histogram = *macd - *signal;
}


void calculate_bollinger_bands(const TradingPair *pair, double *upper, double *middle, double *lower) {
    if (!pair || !upper || !middle || !lower) {
        return;
    }
    
    *upper = 0.0;
    *middle = 0.0;
    *lower = 0.0;
    
    const double *prices = NULL;
    int count = 0;
    int period = 20;
    
    
    if (pair->historical_1h_loaded && pair->historical_1h_count >= period) {
        prices = pair->historical_1h;
        count = pair->historical_1h_count;
    } else if (pair->historical_loaded && pair->historical_count >= period) {
        prices = pair->historical_prices;
        count = pair->historical_count;
    } else {
        return;
    }
    
    
    double sum = 0.0;
    int start_idx = count - period;
    for (int i = start_idx; i < count; i++) {
        sum += prices[i];
    }
    *middle = sum / period;
    
    
    double variance = 0.0;
    for (int i = start_idx; i < count; i++) {
        double diff = prices[i] - *middle;
        variance += diff * diff;
    }
    variance /= period;
    double std_dev = sqrt(variance);
    
    
    *upper = *middle + (2.0 * std_dev);
    *lower = *middle - (2.0 * std_dev);
}


int detect_ema_cross(const TradingPair *pair, int fast_period, int slow_period) {
    if (!pair) {
        return 0;
    }
    
    const double *prices = NULL;
    int count = 0;
    
    
    if (pair->historical_1d_loaded && pair->historical_1d_count >= slow_period) {
        prices = pair->historical_1d;
        count = pair->historical_1d_count;
    } else if (pair->historical_1h_loaded && pair->historical_1h_count >= slow_period) {
        prices = pair->historical_1h;
        count = pair->historical_1h_count;
    } else {
        return 0;
    }
    
    
    double fast_ema = calculate_ema(prices, count, fast_period);
    double slow_ema = calculate_ema(prices, count, slow_period);
    
    
    if (count < slow_period + 1) {
        return 0;
    }
    double fast_ema_prev = calculate_ema(prices, count - 1, fast_period);
    double slow_ema_prev = calculate_ema(prices, count - 1, slow_period);
    
    
    if (fast_ema > slow_ema && fast_ema_prev <= slow_ema_prev) {
        return 1;  
    } else if (fast_ema < slow_ema && fast_ema_prev >= slow_ema_prev) {
        return -1; 
    }
    
    return 0; 
}


bool detect_double_bottom(const double *prices, int count, int *pattern_idx) {
    if (!prices || count < 20 || !pattern_idx) {
        return false;
    }
    
    *pattern_idx = -1;
    
    
    for (int i = 10; i < count - 10; i++) {
        double current_price = prices[i];
        
        
        bool is_local_min = true;
        for (int j = i - 5; j <= i + 5; j++) {
            if (j != i && prices[j] < current_price) {
                is_local_min = false;
                break;
            }
        }
        
        if (!is_local_min) continue;
        
        
        for (int j = i + 10; j < count - 5; j++) {
            double potential_match = prices[j];
            
            
            double diff_percent = fabs(potential_match - current_price) / current_price;
            if (diff_percent < 0.02) {
                
                double max_between = current_price;
                for (int k = i; k <= j; k++) {
                    if (prices[k] > max_between) {
                        max_between = prices[k];
                    }
                }
                
                
                if ((max_between - current_price) / current_price > 0.03) {
                    *pattern_idx = j;
                    return true;
                }
            }
        }
    }
    
    return false;
}


bool detect_double_top(const double *prices, int count, int *pattern_idx) {
    if (!prices || count < 20 || !pattern_idx) {
        return false;
    }
    
    *pattern_idx = -1;
    
    
    for (int i = 10; i < count - 10; i++) {
        double current_price = prices[i];
        
        
        bool is_local_max = true;
        for (int j = i - 5; j <= i + 5; j++) {
            if (j != i && prices[j] > current_price) {
                is_local_max = false;
                break;
            }
        }
        
        if (!is_local_max) continue;
        
        
        for (int j = i + 10; j < count - 5; j++) {
            double potential_match = prices[j];
            
            
            double diff_percent = fabs(potential_match - current_price) / current_price;
            if (diff_percent < 0.02) {
                
                double min_between = current_price;
                for (int k = i; k <= j; k++) {
                    if (prices[k] < min_between) {
                        min_between = prices[k];
                    }
                }
                
                
                if ((current_price - min_between) / current_price > 0.03) {
                    *pattern_idx = j;
                    return true;
                }
            }
        }
    }
    
    return false;
}


bool detect_head_shoulders(const double *prices, int count, int *pattern_idx) {
    if (!prices || count < 30 || !pattern_idx) {
        return false;
    }
    
    *pattern_idx = -1;
    
    
    for (int head = 15; head < count - 15; head++) {
        double head_price = prices[head];
        
        
        bool is_local_max = true;
        for (int j = head - 5; j <= head + 5; j++) {
            if (j != head && prices[j] > head_price) {
                is_local_max = false;
                break;
            }
        }
        
        if (!is_local_max) continue;
        
        
        for (int left = head - 15; left < head - 5; left++) {
            if (prices[left] < head_price * 0.95) continue; 
            
            
            for (int right = head + 5; right < head + 15 && right < count; right++) {
                if (prices[right] < head_price * 0.95) continue;
                
                
                double shoulder_diff = fabs(prices[left] - prices[right]) / prices[left];
                if (shoulder_diff < 0.03) {
                    *pattern_idx = right;
                    return true;
                }
            }
        }
    }
    
    return false;
}


bool detect_inverse_head_shoulders(const double *prices, int count, int *pattern_idx) {
    if (!prices || count < 30 || !pattern_idx) {
        return false;
    }
    
    *pattern_idx = -1;
    
    
    for (int head = 15; head < count - 15; head++) {
        double head_price = prices[head];
        
        
        bool is_local_min = true;
        for (int j = head - 5; j <= head + 5; j++) {
            if (j != head && prices[j] < head_price) {
                is_local_min = false;
                break;
            }
        }
        
        if (!is_local_min) continue;
        
        
        for (int left = head - 15; left < head - 5; left++) {
            if (prices[left] > head_price * 1.05) continue; 
            
            
            for (int right = head + 5; right < head + 15 && right < count; right++) {
                if (prices[right] > head_price * 1.05) continue;
                
                
                double shoulder_diff = fabs(prices[left] - prices[right]) / prices[left];
                if (shoulder_diff < 0.03) {
                    *pattern_idx = right;
                    return true;
                }
            }
        }
    }
    
    return false;
}


int calculate_trend_multi_timeframe(const TradingPair *pair) {
    if (!pair) {
        return 0;
    }
    
    int trend_1h = 0, trend_4h = 0, trend_1d = 0;
    
    
    if (pair->historical_1h_loaded && pair->historical_1h_count >= 10) {
        int count = pair->historical_1h_count;
        double avg_early = 0.0, avg_recent = 0.0;
        int period = 10;
        
        for (int i = count - period * 2; i < count - period; i++) {
            if (i >= 0) avg_early += pair->historical_1h[i];
        }
        avg_early /= period;
        
        for (int i = count - period; i < count; i++) {
            if (i >= 0) avg_recent += pair->historical_1h[i];
        }
        avg_recent /= period;
        
        if (avg_recent > avg_early * 1.01) trend_1h = 1;
        else if (avg_recent < avg_early * 0.99) trend_1h = -1;
    }
    
    
    if (pair->historical_4h_loaded && pair->historical_4h_count >= 10) {
        int count = pair->historical_4h_count;
        double avg_early = 0.0, avg_recent = 0.0;
        int period = 10;
        
        for (int i = count - period * 2; i < count - period; i++) {
            if (i >= 0) avg_early += pair->historical_4h[i];
        }
        avg_early /= period;
        
        for (int i = count - period; i < count; i++) {
            if (i >= 0) avg_recent += pair->historical_4h[i];
        }
        avg_recent /= period;
        
        if (avg_recent > avg_early * 1.01) trend_4h = 1;
        else if (avg_recent < avg_early * 0.99) trend_4h = -1;
    }
    
    
    if (pair->historical_1d_loaded && pair->historical_1d_count >= 10) {
        int count = pair->historical_1d_count;
        double avg_early = 0.0, avg_recent = 0.0;
        int period = 10;
        
        for (int i = count - period * 2; i < count - period; i++) {
            if (i >= 0) avg_early += pair->historical_1d[i];
        }
        avg_early /= period;
        
        for (int i = count - period; i < count; i++) {
            if (i >= 0) avg_recent += pair->historical_1d[i];
        }
        avg_recent /= period;
        
        if (avg_recent > avg_early * 1.01) trend_1d = 1;
        else if (avg_recent < avg_early * 0.99) trend_1d = -1;
    }
    
    
    int total = trend_1h + trend_4h + trend_1d;
    if (total >= 2) return 1;      
    if (total <= -2) return -1;    
    return 0;                       
}


double calculate_profit_probability(const TradingPair *pair) {
    if (!pair) {
        return 0.5;
    }
    
    double score = 50.0;  
    double total_weight = 1.0;
    
    
    double rsi = portfolio_calculate_rsi(pair);
    if (rsi < 30) {
        score += 20.0 * 1.5;
        total_weight += 1.5;
    } else if (rsi > 70) {
        score -= 20.0 * 1.5;
        total_weight += 1.5;
    }
    
    
    double macd, signal, histogram;
    calculate_macd(pair, &macd, &signal, &histogram);
    if (histogram > 0 && macd > signal) {
        score += 15.0 * 1.3;
        total_weight += 1.3;
    } else if (histogram < 0 && macd < signal) {
        score -= 15.0 * 1.3;
        total_weight += 1.3;
    }
    
    
    double bb_upper, bb_middle, bb_lower;
    calculate_bollinger_bands(pair, &bb_upper, &bb_middle, &bb_lower);
    if (bb_lower > 0 && pair->current_price < bb_lower * 1.02) {
        score += 15.0 * 1.2;
        total_weight += 1.2;
    } else if (bb_upper > 0 && pair->current_price > bb_upper * 0.98) {
        score -= 15.0 * 1.2;
        total_weight += 1.2;
    }
    
    
    int pattern_idx;
    const double *prices = pair->historical_1h_loaded ? pair->historical_1h : pair->historical_prices;
    int count = pair->historical_1h_loaded ? pair->historical_1h_count : pair->historical_count;
    
    if (detect_double_bottom(prices, count, &pattern_idx)) {
        score += 25.0 * 1.8;
        total_weight += 1.8;
    }
    if (detect_inverse_head_shoulders(prices, count, &pattern_idx)) {
        score += 30.0 * 2.0;
        total_weight += 2.0;
    }
    if (detect_double_top(prices, count, &pattern_idx)) {
        score -= 25.0 * 1.8;
        total_weight += 1.8;
    }
    if (detect_head_shoulders(prices, count, &pattern_idx)) {
        score -= 30.0 * 2.0;
        total_weight += 2.0;
    }
    
    
    int ema_cross = detect_ema_cross(pair, 50, 200);
    if (ema_cross == 1) {
        score += 35.0 * 2.5;
        total_weight += 2.5;
    } else if (ema_cross == -1) {
        score -= 35.0 * 2.5;
        total_weight += 2.5;
    }
    
    
    int mtf_trend = calculate_trend_multi_timeframe(pair);
    if (mtf_trend == 1) {
        score += 10.0 * 1.0;
        total_weight += 1.0;
    } else if (mtf_trend == -1) {
        score -= 10.0 * 1.0;
        total_weight += 1.0;
    }
    
    
    double volatility = portfolio_calculate_volatility(pair);
    if (volatility < 0.03) {
        score += 5.0 * 0.8;
        total_weight += 0.8;
    } else if (volatility > 0.10) {
        score -= 5.0 * 0.8;
        total_weight += 0.8;
    }
    
    
    double normalized = score / total_weight;
    if (normalized < 0) normalized = 0;
    if (normalized > 100) normalized = 100;
    
    
    return normalized / 100.0;
}


void update_all_indicators(TradingPair *pair) {
    if (!pair) {
        return;
    }
    
    
    const double *prices = pair->historical_1h_loaded ? pair->historical_1h : pair->historical_prices;
    int count = pair->historical_1h_loaded ? pair->historical_1h_count : pair->historical_count;
    
    if (count > 0) {
        pair->ema_12 = calculate_ema(prices, count, 12);
        pair->ema_26 = calculate_ema(prices, count, 26);
        pair->ema_50 = calculate_ema(prices, count, 50);
        pair->ema_200 = calculate_ema(prices, count, 200);
    }
    
    
    calculate_macd(pair, &pair->macd, &pair->macd_signal, NULL);
    
    
    calculate_bollinger_bands(pair, &pair->bb_upper, &pair->bb_middle, &pair->bb_lower);
    
    
    pair->profit_probability = calculate_profit_probability(pair);
    
    
    pair->detected_patterns[0] = '\0';
    pair->pattern_count = 0;
    
    int pattern_idx;
    if (detect_double_bottom(prices, count, &pattern_idx)) {
        if (pair->pattern_count > 0) strcat(pair->detected_patterns, ", ");
        strcat(pair->detected_patterns, "Double Bottom");
        pair->pattern_count++;
    }
    if (detect_double_top(prices, count, &pattern_idx)) {
        if (pair->pattern_count > 0) strcat(pair->detected_patterns, ", ");
        strcat(pair->detected_patterns, "Double Top");
        pair->pattern_count++;
    }
    if (detect_inverse_head_shoulders(prices, count, &pattern_idx)) {
        if (pair->pattern_count > 0) strcat(pair->detected_patterns, ", ");
        strcat(pair->detected_patterns, "Inv H&amp;S");  
        pair->pattern_count++;
    }
    if (detect_head_shoulders(prices, count, &pattern_idx)) {
        if (pair->pattern_count > 0) strcat(pair->detected_patterns, ", ");
        strcat(pair->detected_patterns, "H&amp;S");  
        pair->pattern_count++;
    }
    
    int ema_cross = detect_ema_cross(pair, 50, 200);
    if (ema_cross == 1) {
        if (pair->pattern_count > 0) strcat(pair->detected_patterns, ", ");
        strcat(pair->detected_patterns, "Golden Cross");
        pair->pattern_count++;
    } else if (ema_cross == -1) {
        if (pair->pattern_count > 0) strcat(pair->detected_patterns, ", ");
        strcat(pair->detected_patterns, "Death Cross");
        pair->pattern_count++;
    }
    
    if (pair->pattern_count == 0) {
        strcpy(pair->detected_patterns, "None");
    }
    
    
    analyze_scalping_signals(pair);
}




void analyze_scalping_signals(TradingPair *pair) {
    if (!pair) return;
    
    
    pair->scalp_trend = 0.0;
    pair->scalp_momentum = 0.0;
    strcpy(pair->scalp_signal, "WAIT");
    
    
    if (!pair->historical_5m_loaded || pair->historical_5m_count < 20) {
        return;
    }
    
    const double *prices_5m = pair->historical_5m;
    int count_5m = pair->historical_5m_count;
    
    
    double ema_5 = calculate_ema(prices_5m, count_5m, 5);   
    double ema_10 = calculate_ema(prices_5m, count_5m, 10); 
    double ema_20 = calculate_ema(prices_5m, count_5m, 20); 
    double ema_50 = calculate_ema(prices_5m, count_5m, 50); 
    
    
    double trend_score = 0.0;
    if (ema_5 > ema_10) trend_score += 0.25;
    if (ema_10 > ema_20) trend_score += 0.25;
    if (ema_20 > ema_50) trend_score += 0.25;
    if (pair->current_price > ema_5) trend_score += 0.25;
    
    pair->scalp_trend = (trend_score >= 0.5) ? 1.0 : (trend_score <= 0.25) ? -1.0 : 0.0;
    
    
    if (count_5m >= 10) {
        double sum_changes = 0.0;
        int momentum_period = 10;
        for (int i = count_5m - momentum_period; i < count_5m - 1; i++) {
            if (i >= 0) {
                sum_changes += prices_5m[i + 1] - prices_5m[i];
            }
        }
        pair->scalp_momentum = sum_changes / (prices_5m[count_5m - 1] * momentum_period) * 100.0;
    }
    
    
    double scalp_rsi = 50.0;
    if (count_5m >= 14) {
        double gains = 0.0, losses = 0.0;
        for (int i = count_5m - 14; i < count_5m - 1; i++) {
            if (i >= 0) {
                double change = prices_5m[i + 1] - prices_5m[i];
                if (change > 0) gains += change;
                else losses += fabs(change);
            }
        }
        double avg_gain = gains / 14.0;
        double avg_loss = losses / 14.0;
        if (avg_loss > 0) {
            scalp_rsi = 100.0 - (100.0 / (1.0 + (avg_gain / avg_loss)));
        }
    }
    
    
    bool confirmed_15m = false;
    if (pair->historical_15m_loaded && pair->historical_15m_count >= 20) {
        double ema_15m_fast = calculate_ema(pair->historical_15m, pair->historical_15m_count, 10);
        double ema_15m_slow = calculate_ema(pair->historical_15m, pair->historical_15m_count, 20);
        
        if (pair->scalp_trend > 0 && ema_15m_fast > ema_15m_slow) {
            confirmed_15m = true;
        } else if (pair->scalp_trend < 0 && ema_15m_fast < ema_15m_slow) {
            confirmed_15m = true;
        }
    }
    
    
    if (pair->scalp_trend > 0 && scalp_rsi < 70 && pair->scalp_momentum > 0.005) {
        
        if (confirmed_15m || pair->scalp_momentum > 0.015) {
            strcpy(pair->scalp_signal, "BUY NOW");
        } else {
            strcpy(pair->scalp_signal, "BUY SIGNAL");
        }
    } else if (pair->scalp_trend < 0 && scalp_rsi > 30 && pair->scalp_momentum < -0.005) {
        
        if (confirmed_15m || pair->scalp_momentum < -0.015) {
            strcpy(pair->scalp_signal, "SELL NOW");
        } else {
            strcpy(pair->scalp_signal, "SELL SIGNAL");
        }
    } else if (pair->scalp_trend > 0 && scalp_rsi < 40) {
        strcpy(pair->scalp_signal, "OVERSOLD - BUY DIP");
    } else if (pair->scalp_trend < 0 && scalp_rsi > 60) {
        strcpy(pair->scalp_signal, "OVERBOUGHT - SELL BOUNCE");
    } else if (fabs(pair->scalp_momentum) < 0.003) {
        
        strcpy(pair->scalp_signal, "RANGING - WAIT");
    } else {
        strcpy(pair->scalp_signal, "HOLD POSITION");
    }
    
    
    printf("Scalp: %s | Trend: %.2f | Mom: %.2f%% | RSI: %.1f | Signal: %s\n",
           pair->symbol, pair->scalp_trend, pair->scalp_momentum * 100, scalp_rsi, pair->scalp_signal);
    fflush(stdout);
}


int get_scalping_trend(const TradingPair *pair) {
    if (!pair) return 0;
    return (int)pair->scalp_trend;
}


double get_scalping_momentum(const TradingPair *pair) {
    if (!pair) return 0.0;
    return pair->scalp_momentum;
}


const char* get_scalping_signal(const TradingPair *pair) {
    if (!pair) return "NO DATA";
    return pair->scalp_signal;
}


bool is_scalp_buy_signal(const TradingPair *pair) {
    if (!pair) return false;
    return (strstr(pair->scalp_signal, "BUY") != NULL);
}


bool is_scalp_sell_signal(const TradingPair *pair) {
    if (!pair) return false;
    return (strstr(pair->scalp_signal, "SELL") != NULL);
}
