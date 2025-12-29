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

#include "portfolio/scalping_bot.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>


BotManager* bot_manager_create(void) {
    BotManager *manager = calloc(1, sizeof(BotManager));
    if (!manager) return NULL;
    
    manager->bot_count = 0;
    for (int i = 0; i < MAX_BOTS; i++) {
        manager->bots[i].active = false;
        manager->bots[i].status = BOT_STOPPED;
    }
    
    return manager;
}


void bot_manager_destroy(BotManager *manager) {
    if (manager) {
        free(manager);
    }
}


int bot_add(BotManager *manager, const char *symbol, double initial_balance, double trade_amount) {
    if (!manager || !symbol || manager->bot_count >= MAX_BOTS) {
        return -1;
    }
    
    
    int index = -1;
    for (int i = 0; i < MAX_BOTS; i++) {
        if (!manager->bots[i].active) {
            index = i;
            break;
        }
    }
    
    if (index == -1) return -1;
    
    ScalpingBot *bot = &manager->bots[index];
    
    
    strncpy(bot->symbol, symbol, MAX_SYMBOL_LEN - 1);
    bot->symbol[MAX_SYMBOL_LEN - 1] = '\0';
    bot->active = true;
    bot->status = BOT_STOPPED;
    
    bot->initial_balance = initial_balance;
    bot->current_balance = initial_balance;
    bot->trade_amount_usd = trade_amount;
    
    bot->current_position = 0.0;
    bot->avg_buy_price = 0.0;
    
    bot->total_trades = 0;
    bot->winning_trades = 0;
    bot->losing_trades = 0;
    bot->total_profit = 0.0;
    bot->total_fees_paid = 0.0;
    bot->max_drawdown = 0.0;
    bot->win_rate = 0.0;
    
    bot->trade_count = 0;
    bot->trade_index = 0;
    
    bot->created_at = time(NULL);
    bot->last_trade_time = 0;
    
    manager->bot_count++;
    
    printf("Bot #%d created for %s with $%.2f balance\n", index, symbol, initial_balance);
    
    return index;
}


void bot_remove(BotManager *manager, int bot_index) {
    if (!manager || bot_index < 0 || bot_index >= MAX_BOTS) return;
    
    ScalpingBot *bot = &manager->bots[bot_index];
    if (bot->active) {
        bot->active = false;
        bot->status = BOT_STOPPED;
        manager->bot_count--;
        printf("Bot #%d removed\n", bot_index);
    }
}


void bot_start(BotManager *manager, int bot_index) {
    if (!manager || bot_index < 0 || bot_index >= MAX_BOTS) return;
    
    ScalpingBot *bot = &manager->bots[bot_index];
    if (bot->active) {
        bot->status = BOT_RUNNING;
        printf("Bot #%d started for %s\n", bot_index, bot->symbol);
    }
}


void bot_stop(BotManager *manager, int bot_index) {
    if (!manager || bot_index < 0 || bot_index >= MAX_BOTS) return;
    
    ScalpingBot *bot = &manager->bots[bot_index];
    if (bot->active) {
        bot->status = BOT_STOPPED;
        printf("Bot #%d stopped\n", bot_index);
    }
}


void bot_pause(BotManager *manager, int bot_index) {
    if (!manager || bot_index < 0 || bot_index >= MAX_BOTS) return;
    
    ScalpingBot *bot = &manager->bots[bot_index];
    if (bot->active && bot->status == BOT_RUNNING) {
        bot->status = BOT_PAUSED;
        printf("Bot #%d paused\n", bot_index);
    }
}


void bot_reset(BotManager *manager, int bot_index) {
    if (!manager || bot_index < 0 || bot_index >= MAX_BOTS) return;
    
    ScalpingBot *bot = &manager->bots[bot_index];
    if (!bot->active) return;
    
    bot->current_balance = bot->initial_balance;
    bot->current_position = 0.0;
    bot->avg_buy_price = 0.0;
    
    bot->total_trades = 0;
    bot->winning_trades = 0;
    bot->losing_trades = 0;
    bot->total_profit = 0.0;
    bot->total_fees_paid = 0.0;
    bot->max_drawdown = 0.0;
    bot->win_rate = 0.0;
    
    bot->trade_count = 0;
    bot->trade_index = 0;
    bot->last_trade_time = 0;
    
    printf("Bot #%d reset\n", bot_index);
}


void bot_execute_buy(ScalpingBot *bot, double price, const char *signal) {
    if (!bot || price <= 0) return;
    
    
    if (bot->current_balance < bot->trade_amount_usd) {
        printf("Bot %s: Insufficient balance for buy (%.2f < %.2f)\n", 
               bot->symbol, bot->current_balance, bot->trade_amount_usd);
        return;
    }
    
    
    double gross_quantity = bot->trade_amount_usd / price;
    double fee = gross_quantity * TAKER_FEE;  
    double net_quantity = gross_quantity - fee;
    double total_cost = bot->trade_amount_usd;
    
    
    double new_position = bot->current_position + net_quantity;
    double new_avg_price = ((bot->current_position * bot->avg_buy_price) + total_cost) / new_position;
    
    bot->current_position = new_position;
    bot->avg_buy_price = new_avg_price;
    bot->current_balance -= total_cost;
    bot->total_fees_paid += fee * price;
    
    
    int idx = bot->trade_index;
    bot->trades[idx].timestamp = time(NULL);
    strcpy(bot->trades[idx].action, "BUY");
    bot->trades[idx].price = price;
    bot->trades[idx].quantity = net_quantity;
    bot->trades[idx].fee = fee * price;
    bot->trades[idx].total_cost = total_cost;
    bot->trades[idx].balance_after = bot->current_balance;
    strncpy(bot->trades[idx].signal, signal, 63);
    bot->trades[idx].signal[63] = '\0';
    
    bot->trade_index = (bot->trade_index + 1) % MAX_TRADES_HISTORY;
    if (bot->trade_count < MAX_TRADES_HISTORY) {
        bot->trade_count++;
    }
    
    bot->total_trades++;
    bot->last_trade_time = time(NULL);
    
    printf("Bot %s BUY: %.6f @ $%.2f (fee: $%.2f) | Balance: $%.2f | Position: %.6f\n",
           bot->symbol, net_quantity, price, fee * price, bot->current_balance, bot->current_position);
}


void bot_execute_sell(ScalpingBot *bot, double price, const char *signal) {
    if (!bot || price <= 0) return;
    
    
    if (bot->current_position <= 0.0001) {
        return;
    }
    
    
    double sell_quantity = bot->current_position;
    double gross_proceeds = sell_quantity * price;
    double fee = gross_proceeds * TAKER_FEE;
    double net_proceeds = gross_proceeds - fee;
    
    
    double cost_basis = bot->current_position * bot->avg_buy_price;
    double profit = net_proceeds - cost_basis;
    
    
    bot->current_balance += net_proceeds;
    bot->total_profit += profit;
    bot->total_fees_paid += fee;
    
    if (profit > 0) {
        bot->winning_trades++;
    } else {
        bot->losing_trades++;
    }
    
    
    int idx = bot->trade_index;
    bot->trades[idx].timestamp = time(NULL);
    strcpy(bot->trades[idx].action, "SELL");
    bot->trades[idx].price = price;
    bot->trades[idx].quantity = sell_quantity;
    bot->trades[idx].fee = fee;
    bot->trades[idx].total_cost = net_proceeds;
    bot->trades[idx].balance_after = bot->current_balance;
    strncpy(bot->trades[idx].signal, signal, 63);
    bot->trades[idx].signal[63] = '\0';
    
    bot->trade_index = (bot->trade_index + 1) % MAX_TRADES_HISTORY;
    if (bot->trade_count < MAX_TRADES_HISTORY) {
        bot->trade_count++;
    }
    
    bot->total_trades++;
    bot->last_trade_time = time(NULL);
    
    
    bot->current_position = 0.0;
    bot->avg_buy_price = 0.0;
    
    
    double drawdown = (bot->initial_balance - bot->current_balance) / bot->initial_balance;
    if (drawdown > bot->max_drawdown) {
        bot->max_drawdown = drawdown;
    }
    
    printf("Bot %s SELL: %.6f @ $%.2f (fee: $%.2f) | P/L: $%+.2f | Balance: $%.2f\n",
           bot->symbol, sell_quantity, price, fee, profit, bot->current_balance);
}


void bot_process_signal(BotManager *manager, int bot_index, const TradingPair *pair) {
    if (!manager || !pair || bot_index < 0 || bot_index >= MAX_BOTS) return;
    
    ScalpingBot *bot = &manager->bots[bot_index];
    
    
    if (!bot->active || bot->status != BOT_RUNNING) return;
    
    
    char upper_symbol[MAX_SYMBOL_LEN];
    strncpy(upper_symbol, pair->symbol, MAX_SYMBOL_LEN);
    for (int i = 0; upper_symbol[i]; i++) {
        upper_symbol[i] = toupper((unsigned char)upper_symbol[i]);
    }
    
    char upper_bot_symbol[MAX_SYMBOL_LEN];
    strncpy(upper_bot_symbol, bot->symbol, MAX_SYMBOL_LEN);
    for (int i = 0; upper_bot_symbol[i]; i++) {
        upper_bot_symbol[i] = toupper((unsigned char)upper_bot_symbol[i]);
    }
    
    if (strcmp(upper_symbol, upper_bot_symbol) != 0) return;
    
    
    if (!pair->historical_5m_loaded || pair->current_price <= 0) {
        printf("Warning: Bot %s waiting for data (5m loaded: %d, price: %.2f)\n", 
               bot->symbol, pair->historical_5m_loaded, pair->current_price);
        return;
    }
    
    
    time_t now = time(NULL);
    if (bot->last_trade_time > 0 && (now - bot->last_trade_time) < 30) {
        int seconds_left = 30 - (now - bot->last_trade_time);
        printf("Bot %s in cooldown (%d seconds left)\n", bot->symbol, seconds_left);
        return;
    }
    
    const char *signal = pair->scalp_signal;
    
    printf("Bot %s checking signal: '%s' | Position: %.6f | Balance: $%.2f\n",
           bot->symbol, signal, bot->current_position, bot->current_balance);
    fflush(stdout);
    
    
    if (strstr(signal, "BUY NOW") != NULL && bot->current_position < 0.0001) {
        printf("   -> Executing BUY NOW\n");
        bot_execute_buy(bot, pair->current_price, signal);
    }
    else if (strstr(signal, "SELL NOW") != NULL && bot->current_position > 0.0001) {
        printf("   -> Executing SELL NOW\n");
        bot_execute_sell(bot, pair->current_price, signal);
    }
    else if (strstr(signal, "BUY DIP") != NULL && bot->current_position < 0.0001) {
        printf("   -> Executing BUY DIP\n");
        bot_execute_buy(bot, pair->current_price, signal);
    }
    else if (strstr(signal, "SELL BOUNCE") != NULL && bot->current_position > 0.0001) {
        printf("   -> Executing SELL BOUNCE\n");
        bot_execute_sell(bot, pair->current_price, signal);
    }
    else if (strstr(signal, "BUY SIGNAL") != NULL && bot->current_position < 0.0001) {
        printf("   -> Executing BUY SIGNAL\n");
        bot_execute_buy(bot, pair->current_price, signal);
    }
    else if (strstr(signal, "SELL SIGNAL") != NULL && bot->current_position > 0.0001) {
        printf("   -> Executing SELL SIGNAL\n");
        bot_execute_sell(bot, pair->current_price, signal);
    }
    else {
        printf("   -> No action (signal doesn't match or wrong position state)\n");
    }
}


void bot_update_statistics(ScalpingBot *bot) {
    if (!bot) return;
    
    if (bot->total_trades > 0) {
        bot->win_rate = (double)bot->winning_trades / (bot->winning_trades + bot->losing_trades) * 100.0;
    } else {
        bot->win_rate = 0.0;
    }
}


double bot_get_total_value(const ScalpingBot *bot, double current_price) {
    if (!bot) return 0.0;
    
    double position_value = bot->current_position * current_price;
    return bot->current_balance + position_value;
}


double bot_get_roi(const ScalpingBot *bot, double current_price) {
    if (!bot || bot->initial_balance <= 0) return 0.0;
    
    double total_value = bot_get_total_value(bot, current_price);
    return ((total_value - bot->initial_balance) / bot->initial_balance) * 100.0;
}


void bot_manager_save(const BotManager *manager) {
    if (!manager) return;
    printf("Bot manager save: %d active bots\n", manager->bot_count);
    
}


bool bot_manager_load(BotManager *manager) {
    if (!manager) return false;
    printf("Bot manager load\n");
    
    return true;
}
