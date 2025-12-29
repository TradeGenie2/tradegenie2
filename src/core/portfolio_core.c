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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <json-c/json.h>
#include <math.h>

Portfolio* portfolio_create(void) {
    Portfolio *portfolio = calloc(1, sizeof(Portfolio));
    if (!portfolio) {
        return NULL;
    }
    portfolio->pair_count = 0;
    return portfolio;
}

void portfolio_destroy(Portfolio *portfolio) {
    if (portfolio) {
        free(portfolio);
    }
}

void portfolio_init_default(Portfolio *portfolio) {
    if (!portfolio) return;
    
    portfolio->pair_count = 4;  
    
    
    for (int i = 0; i < 4; i++) {
        portfolio->pairs[i].current_price = 0.0;
        portfolio->pairs[i].history_count = 0;
        portfolio->pairs[i].history_index = 0;
        portfolio->pairs[i].historical_count = 0;
        portfolio->pairs[i].historical_loaded = false;
        portfolio->pairs[i].last_historical_fetch = 0;
        
        
        
        portfolio->pairs[i].historical_5m_count = 0;
        portfolio->pairs[i].historical_5m_loaded = false;
        portfolio->pairs[i].last_5m_fetch = 0;
        portfolio->pairs[i].historical_15m_count = 0;
        portfolio->pairs[i].historical_15m_loaded = false;
        portfolio->pairs[i].last_15m_fetch = 0;
        
        portfolio->pairs[i].historical_1h_count = 0;
        portfolio->pairs[i].historical_1h_loaded = false;
        portfolio->pairs[i].last_1h_fetch = 0;
        portfolio->pairs[i].historical_4h_count = 0;
        portfolio->pairs[i].historical_4h_loaded = false;
        portfolio->pairs[i].last_4h_fetch = 0;
        portfolio->pairs[i].historical_1d_count = 0;
        portfolio->pairs[i].historical_1d_loaded = false;
        portfolio->pairs[i].last_1d_fetch = 0;
        
        
        portfolio->pairs[i].ema_12 = 0.0;
        portfolio->pairs[i].ema_26 = 0.0;
        portfolio->pairs[i].ema_50 = 0.0;
        portfolio->pairs[i].ema_200 = 0.0;
        portfolio->pairs[i].macd = 0.0;
        portfolio->pairs[i].macd_signal = 0.0;
        portfolio->pairs[i].bb_upper = 0.0;
        portfolio->pairs[i].bb_middle = 0.0;
        portfolio->pairs[i].bb_lower = 0.0;
        
        
        portfolio->pairs[i].scalp_trend = 0.0;
        portfolio->pairs[i].scalp_momentum = 0.0;
        strcpy(portfolio->pairs[i].scalp_signal, "WAIT");
        
        
        portfolio->pairs[i].profit_probability = 0.5;
        portfolio->pairs[i].detected_patterns[0] = '\0';
        portfolio->pairs[i].pattern_count = 0;
    }
    
    
    strcpy(portfolio->pairs[0].symbol, "btcusdt");
    portfolio->pairs[0].bought_price = 30000.00;
    portfolio->pairs[0].quantity = 2.1515;
    portfolio->pairs[0].position_type = POSITION_LONG;
    
    
    strcpy(portfolio->pairs[1].symbol, "ethusdt");
    portfolio->pairs[1].bought_price = 1800.00;
    portfolio->pairs[1].quantity = 5.5;
    portfolio->pairs[1].position_type = POSITION_LONG;
    
    
    strcpy(portfolio->pairs[2].symbol, "adausdt");
    portfolio->pairs[2].bought_price = 0.45;
    portfolio->pairs[2].quantity = 10000.0;
    portfolio->pairs[2].position_type = POSITION_LONG;
    
    
    strcpy(portfolio->pairs[3].symbol, "dogeusdt");
    portfolio->pairs[3].bought_price = 0.10;  
    portfolio->pairs[3].quantity = 50000.0;
    portfolio->pairs[3].position_type = POSITION_SHORT;
}

char* portfolio_get_file_path(void) {
    const char *home_dir = getenv("HOME");
    if (!home_dir) {
        home_dir = ".";
    }
    
    char config_dir[1024];
    snprintf(config_dir, sizeof(config_dir), "%s/.config/portfolio", home_dir);
    mkdir(config_dir, 0755);
    
    char *filepath = malloc(1024);
    snprintf(filepath, 1024, "%s/portfolio.json", config_dir);
    return filepath;
}

void portfolio_save(Portfolio *portfolio) {
    if (!portfolio) return;
    
    char *filepath = portfolio_get_file_path();
    
    struct json_object *root = json_object_new_object();
    struct json_object *pairs_array = json_object_new_array();
    
    for (int i = 0; i < portfolio->pair_count; i++) {
        struct json_object *pair_obj = json_object_new_object();
        json_object_object_add(pair_obj, "symbol", json_object_new_string(portfolio->pairs[i].symbol));
        json_object_object_add(pair_obj, "bought_price", json_object_new_double(portfolio->pairs[i].bought_price));
        json_object_object_add(pair_obj, "quantity", json_object_new_double(portfolio->pairs[i].quantity));
        json_object_object_add(pair_obj, "position_type", json_object_new_int(portfolio->pairs[i].position_type));
        json_object_array_add(pairs_array, pair_obj);
    }
    
    json_object_object_add(root, "pairs", pairs_array);
    
    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    FILE *fp = fopen(filepath, "w");
    if (fp) {
        fprintf(fp, "%s", json_str);
        fclose(fp);
        printf("Portfolio saved to: %s\n", filepath);
    } else {
        fprintf(stderr, "Failed to save portfolio: %s\n", strerror(errno));
    }
    
    json_object_put(root);
    free(filepath);
}

bool portfolio_load(Portfolio *portfolio) {
    if (!portfolio) return false;
    
    char *filepath = portfolio_get_file_path();
    FILE *fp = fopen(filepath, "r");
    
    if (!fp) {
        free(filepath);
        return false;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *json_str = malloc(file_size + 1);
    size_t bytes_read = fread(json_str, 1, file_size, fp);
    json_str[bytes_read] = '\0';
    fclose(fp);
    
    struct json_object *root = json_tokener_parse(json_str);
    free(json_str);
    free(filepath);
    
    if (!root) {
        return false;
    }
    
    struct json_object *pairs_array;
    if (!json_object_object_get_ex(root, "pairs", &pairs_array)) {
        json_object_put(root);
        return false;
    }
    
    int array_len = json_object_array_length(pairs_array);
    portfolio->pair_count = (array_len < MAX_PAIRS) ? array_len : MAX_PAIRS;
    
    for (int i = 0; i < portfolio->pair_count; i++) {
        struct json_object *pair_obj = json_object_array_get_idx(pairs_array, i);
        struct json_object *symbol_obj, *bought_price_obj, *quantity_obj, *position_type_obj;
        
        if (json_object_object_get_ex(pair_obj, "symbol", &symbol_obj)) {
            strncpy(portfolio->pairs[i].symbol, json_object_get_string(symbol_obj), MAX_SYMBOL_LEN - 1);
        }
        if (json_object_object_get_ex(pair_obj, "bought_price", &bought_price_obj)) {
            portfolio->pairs[i].bought_price = json_object_get_double(bought_price_obj);
        }
        if (json_object_object_get_ex(pair_obj, "quantity", &quantity_obj)) {
            portfolio->pairs[i].quantity = json_object_get_double(quantity_obj);
        }
        
        if (json_object_object_get_ex(pair_obj, "position_type", &position_type_obj)) {
            portfolio->pairs[i].position_type = json_object_get_int(position_type_obj);
        } else {
            portfolio->pairs[i].position_type = POSITION_LONG;  
        }
        
        portfolio->pairs[i].current_price = 0.0;
        portfolio->pairs[i].history_count = 0;
        portfolio->pairs[i].history_index = 0;
        portfolio->pairs[i].historical_count = 0;
        portfolio->pairs[i].historical_loaded = false;
        portfolio->pairs[i].last_historical_fetch = 0;
    }
    
    json_object_put(root);
    printf("Portfolio loaded: %d pairs\n", portfolio->pair_count);
    return true;
}

int portfolio_add_pair(Portfolio *portfolio, const char *symbol, double bought_price, 
                       double quantity, PositionType position_type) {
    if (!portfolio || portfolio->pair_count >= MAX_PAIRS) {
        return -1;
    }
    
    int index = portfolio->pair_count;
    strncpy(portfolio->pairs[index].symbol, symbol, MAX_SYMBOL_LEN - 1);
    portfolio->pairs[index].symbol[MAX_SYMBOL_LEN - 1] = '\0';
    portfolio->pairs[index].bought_price = bought_price;
    portfolio->pairs[index].quantity = quantity;
    portfolio->pairs[index].position_type = position_type;
    portfolio->pairs[index].current_price = 0.0;
    portfolio->pairs[index].history_count = 0;
    portfolio->pairs[index].history_index = 0;
    portfolio->pairs[index].historical_count = 0;
    portfolio->pairs[index].historical_loaded = false;
    portfolio->pairs[index].last_historical_fetch = 0;
    
    
    
    portfolio->pairs[index].historical_5m_count = 0;
    portfolio->pairs[index].historical_5m_loaded = false;
    portfolio->pairs[index].last_5m_fetch = 0;
    portfolio->pairs[index].historical_15m_count = 0;
    portfolio->pairs[index].historical_15m_loaded = false;
    portfolio->pairs[index].last_15m_fetch = 0;
    
    portfolio->pairs[index].historical_1h_count = 0;
    portfolio->pairs[index].historical_1h_loaded = false;
    portfolio->pairs[index].last_1h_fetch = 0;
    portfolio->pairs[index].historical_4h_count = 0;
    portfolio->pairs[index].historical_4h_loaded = false;
    portfolio->pairs[index].last_4h_fetch = 0;
    portfolio->pairs[index].historical_1d_count = 0;
    portfolio->pairs[index].historical_1d_loaded = false;
    portfolio->pairs[index].last_1d_fetch = 0;
    
    
    portfolio->pairs[index].ema_12 = 0.0;
    portfolio->pairs[index].ema_26 = 0.0;
    portfolio->pairs[index].ema_50 = 0.0;
    portfolio->pairs[index].ema_200 = 0.0;
    portfolio->pairs[index].macd = 0.0;
    portfolio->pairs[index].macd_signal = 0.0;
    portfolio->pairs[index].bb_upper = 0.0;
    portfolio->pairs[index].bb_middle = 0.0;
    portfolio->pairs[index].bb_lower = 0.0;
    
    
    portfolio->pairs[index].scalp_trend = 0.0;
    portfolio->pairs[index].scalp_momentum = 0.0;
    strcpy(portfolio->pairs[index].scalp_signal, "WAIT");
    
    
    portfolio->pairs[index].profit_probability = 0.5;
    portfolio->pairs[index].detected_patterns[0] = '\0';
    portfolio->pairs[index].pattern_count = 0;
    
    portfolio->pair_count++;
    return index;
}

void portfolio_remove_pair(Portfolio *portfolio, int index) {
    if (!portfolio || index < 0 || index >= portfolio->pair_count) {
        return;
    }
    
    for (int i = index; i < portfolio->pair_count - 1; i++) {
        portfolio->pairs[i] = portfolio->pairs[i + 1];
    }
    portfolio->pair_count--;
}

void portfolio_update_pair(Portfolio *portfolio, int index, const char *symbol, 
                          double bought_price, double quantity, PositionType position_type) {
    if (!portfolio || index < 0 || index >= portfolio->pair_count) {
        return;
    }
    
    strncpy(portfolio->pairs[index].symbol, symbol, MAX_SYMBOL_LEN - 1);
    portfolio->pairs[index].symbol[MAX_SYMBOL_LEN - 1] = '\0';
    portfolio->pairs[index].bought_price = bought_price;
    portfolio->pairs[index].quantity = quantity;
    portfolio->pairs[index].position_type = position_type;
}

void portfolio_update_current_price(Portfolio *portfolio, int index, double price) {
    if (!portfolio || index < 0 || index >= portfolio->pair_count) {
        return;
    }
    
    TradingPair *pair = &portfolio->pairs[index];
    pair->current_price = price;
    
    pair->price_history[pair->history_index] = price;
    pair->history_index = (pair->history_index + 1) % PRICE_HISTORY_SIZE;
    if (pair->history_count < PRICE_HISTORY_SIZE) {
        pair->history_count++;
    }
}

double portfolio_get_total_value(const Portfolio *portfolio) {
    if (!portfolio) return 0.0;
    
    double total = 0.0;
    for (int i = 0; i < portfolio->pair_count; i++) {
        double position_value;
        if (portfolio->pairs[i].position_type == POSITION_LONG) {
            
            position_value = portfolio->pairs[i].current_price * portfolio->pairs[i].quantity;
        } else {
            
            
            double entry_value = portfolio->pairs[i].bought_price * portfolio->pairs[i].quantity;
            double current_value = portfolio->pairs[i].current_price * portfolio->pairs[i].quantity;
            position_value = entry_value + (entry_value - current_value);
        }
        total += position_value;
    }
    return total;
}

double portfolio_get_total_cost(const Portfolio *portfolio) {
    if (!portfolio) return 0.0;
    
    double total = 0.0;
    for (int i = 0; i < portfolio->pair_count; i++) {
        total += portfolio->pairs[i].bought_price * portfolio->pairs[i].quantity;
    }
    return total;
}

double portfolio_get_total_profit_loss(const Portfolio *portfolio) {
    if (!portfolio) return 0.0;
    
    double total_pl = 0.0;
    for (int i = 0; i < portfolio->pair_count; i++) {
        double entry_value = portfolio->pairs[i].bought_price * portfolio->pairs[i].quantity;
        double current_value = portfolio->pairs[i].current_price * portfolio->pairs[i].quantity;
        
        if (portfolio->pairs[i].position_type == POSITION_LONG) {
            
            total_pl += (current_value - entry_value);
        } else {
            
            total_pl += (entry_value - current_value);
        }
    }
    return total_pl;
}

double portfolio_get_total_profit_loss_percent(const Portfolio *portfolio) {
    double cost = portfolio_get_total_cost(portfolio);
    if (cost == 0.0) return 0.0;
    return (portfolio_get_total_profit_loss(portfolio) / cost) * 100.0;
}


