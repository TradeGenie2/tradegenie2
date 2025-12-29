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

#ifndef SCALPING_BOT_H
#define SCALPING_BOT_H

#include "portfolio_core.h"
#include <time.h>
#include <stdbool.h>

#define MAX_BOTS 5
#define MAX_TRADES_HISTORY 100


#define MAKER_FEE 0.001   
#define TAKER_FEE 0.001   


typedef enum {
    BOT_STOPPED = 0,
    BOT_RUNNING = 1,
    BOT_PAUSED = 2
} BotStatus;


typedef struct {
    time_t timestamp;
    char action[16];        
    double price;
    double quantity;
    double fee;
    double total_cost;      
    double balance_after;   
    char signal[64];        
} TradeRecord;


typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    bool active;
    BotStatus status;
    
    
    double initial_balance;
    double current_balance;
    double trade_amount_usd;    
    
    
    double current_position;     
    double avg_buy_price;        
    
    
    int total_trades;
    int winning_trades;
    int losing_trades;
    double total_profit;
    double total_fees_paid;
    double max_drawdown;
    double win_rate;
    
    
    TradeRecord trades[MAX_TRADES_HISTORY];
    int trade_count;
    int trade_index;
    
    
    time_t created_at;
    time_t last_trade_time;
    
} ScalpingBot;


typedef struct {
    ScalpingBot bots[MAX_BOTS];
    int bot_count;
} BotManager;


BotManager* bot_manager_create(void);
void bot_manager_destroy(BotManager *manager);


int bot_add(BotManager *manager, const char *symbol, double initial_balance, double trade_amount);
void bot_remove(BotManager *manager, int bot_index);
void bot_start(BotManager *manager, int bot_index);
void bot_stop(BotManager *manager, int bot_index);
void bot_pause(BotManager *manager, int bot_index);
void bot_reset(BotManager *manager, int bot_index);


void bot_process_signal(BotManager *manager, int bot_index, const TradingPair *pair);
void bot_execute_buy(ScalpingBot *bot, double price, const char *signal);
void bot_execute_sell(ScalpingBot *bot, double price, const char *signal);


void bot_update_statistics(ScalpingBot *bot);
double bot_get_total_value(const ScalpingBot *bot, double current_price);
double bot_get_roi(const ScalpingBot *bot, double current_price);


void bot_manager_save(const BotManager *manager);
bool bot_manager_load(BotManager *manager);

#endif 
