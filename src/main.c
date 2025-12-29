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
#include "portfolio/network.h"
#include "portfolio/scalping_bot.h"
#include "ui/ui_factory.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    Portfolio *portfolio;
    NetworkManager *network;
    BotManager *bot_manager;
    UIInterface *ui;
} AppContext;


static void on_price_update(int pair_index, double price, void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    portfolio_update_current_price(ctx->portfolio, pair_index, price);
    
    if (ctx->ui && ctx->ui->update_pair_price) {
        ctx->ui->update_pair_price(pair_index, price, ctx->ui->impl_data);
    }
}

static void on_historical_data(int pair_index, double *prices, int count, void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    if (pair_index >= 0 && pair_index < ctx->portfolio->pair_count) {
        TradingPair *pair = &ctx->portfolio->pairs[pair_index];
        
        int copy_count = (count < HISTORICAL_DATA_SIZE) ? count : HISTORICAL_DATA_SIZE;
        for (int i = 0; i < copy_count; i++) {
            pair->historical_prices[i] = prices[i];
        }
        pair->historical_count = copy_count;
        pair->historical_loaded = true;
        pair->last_historical_fetch = time(NULL);
        
        printf("Loaded %d historical prices for %s\n", copy_count, pair->symbol);
    }
}

static void on_multi_timeframe_data(int pair_index, const char *interval, double *prices, int count, void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    if (pair_index >= 0 && pair_index < ctx->portfolio->pair_count) {
        TradingPair *pair = &ctx->portfolio->pairs[pair_index];
        
        if (strcmp(interval, "5m") == 0) {
            int copy_count = (count < HISTORICAL_DATA_SIZE_5M) ? count : HISTORICAL_DATA_SIZE_5M;
            for (int i = 0; i < copy_count; i++) {
                pair->historical_5m[i] = prices[i];
            }
            pair->historical_5m_count = copy_count;
            pair->historical_5m_loaded = true;
            pair->last_5m_fetch = time(NULL);
            printf("Loaded %d 5m candles for %s (scalping)\n", copy_count, pair->symbol);
        } else if (strcmp(interval, "15m") == 0) {
            int copy_count = (count < HISTORICAL_DATA_SIZE_15M) ? count : HISTORICAL_DATA_SIZE_15M;
            for (int i = 0; i < copy_count; i++) {
                pair->historical_15m[i] = prices[i];
            }
            pair->historical_15m_count = copy_count;
            pair->historical_15m_loaded = true;
            pair->last_15m_fetch = time(NULL);
            printf("Loaded %d 15m candles for %s (scalping)\n", copy_count, pair->symbol);
        } else if (strcmp(interval, "1h") == 0) {
            int copy_count = (count < HISTORICAL_DATA_SIZE_1H) ? count : HISTORICAL_DATA_SIZE_1H;
            for (int i = 0; i < copy_count; i++) {
                pair->historical_1h[i] = prices[i];
            }
            pair->historical_1h_count = copy_count;
            pair->historical_1h_loaded = true;
            pair->last_1h_fetch = time(NULL);
            printf("Loaded %d 1h candles for %s\n", copy_count, pair->symbol);
        } else if (strcmp(interval, "4h") == 0) {
            int copy_count = (count < HISTORICAL_DATA_SIZE_4H) ? count : HISTORICAL_DATA_SIZE_4H;
            for (int i = 0; i < copy_count; i++) {
                pair->historical_4h[i] = prices[i];
            }
            pair->historical_4h_count = copy_count;
            pair->historical_4h_loaded = true;
            pair->last_4h_fetch = time(NULL);
            printf("Loaded %d 4h candles for %s\n", copy_count, pair->symbol);
        } else if (strcmp(interval, "1d") == 0) {
            int copy_count = (count < HISTORICAL_DATA_SIZE_1D) ? count : HISTORICAL_DATA_SIZE_1D;
            for (int i = 0; i < copy_count; i++) {
                pair->historical_1d[i] = prices[i];
            }
            pair->historical_1d_count = copy_count;
            pair->historical_1d_loaded = true;
            pair->last_1d_fetch = time(NULL);
            printf("Loaded %d 1d candles for %s\n", copy_count, pair->symbol);
        }
        
        
        update_all_indicators(pair);
        
        
        if (ctx->bot_manager && pair->current_price > 0) {
            
            if (pair->historical_5m_loaded && pair->historical_5m_count > 20) {
                for (int i = 0; i < MAX_BOTS; i++) {
                    if (ctx->bot_manager->bots[i].active && 
                        ctx->bot_manager->bots[i].status == BOT_RUNNING) {
                        bot_process_signal(ctx->bot_manager, i, pair);
                    }
                }
            }
        }
        
        
        if (ctx->ui && ctx->ui->update_portfolio_display) {
            ctx->ui->update_portfolio_display(ctx->portfolio, ctx->ui->impl_data);
        }
    }
}

static void on_add_pair_callback(const char *symbol, double bought_price, double quantity, 
                                PositionType position_type, void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    int index = portfolio_add_pair(ctx->portfolio, symbol, bought_price, quantity, position_type);
    if (index >= 0) {
        portfolio_save(ctx->portfolio);
        
        
        network_fetch_price(ctx->network, symbol, index, on_price_update, ctx);
        
        
        network_fetch_all_timeframes(ctx->network, symbol, index, on_multi_timeframe_data, ctx);
        
        if (ctx->ui && ctx->ui->update_portfolio_display) {
            ctx->ui->update_portfolio_display(ctx->portfolio, ctx->ui->impl_data);
        }
    }
}

static void on_remove_pair_callback(int pair_index, void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    portfolio_remove_pair(ctx->portfolio, pair_index);
    portfolio_save(ctx->portfolio);
    
    if (ctx->ui && ctx->ui->update_portfolio_display) {
        ctx->ui->update_portfolio_display(ctx->portfolio, ctx->ui->impl_data);
    }
}

static void on_edit_pair_callback(int pair_index, const char *symbol, 
                                  double bought_price, double quantity, 
                                  PositionType position_type, void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    portfolio_update_pair(ctx->portfolio, pair_index, symbol, bought_price, quantity, position_type);
    portfolio_save(ctx->portfolio);
    
    
    network_fetch_price(ctx->network, symbol, pair_index, on_price_update, ctx);
    network_fetch_all_timeframes(ctx->network, symbol, pair_index, on_multi_timeframe_data, ctx);
    
    if (ctx->ui && ctx->ui->update_portfolio_display) {
        ctx->ui->update_portfolio_display(ctx->portfolio, ctx->ui->impl_data);
    }
}

static void on_refresh_callback(void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    printf("Refreshing prices and multi-timeframe data...\n");
    network_fetch_all_prices(ctx->network, ctx->portfolio, on_price_update, ctx);
    
    
    if (ctx->bot_manager) {
        for (int i = 0; i < MAX_BOTS; i++) {
            if (ctx->bot_manager->bots[i].active && 
                ctx->bot_manager->bots[i].status == BOT_RUNNING) {
                
                ScalpingBot *bot = &ctx->bot_manager->bots[i];
                
                
                for (int j = 0; j < ctx->portfolio->pair_count; j++) {
                    TradingPair *pair = &ctx->portfolio->pairs[j];
                    
                    char upper_pair[MAX_SYMBOL_LEN];
                    strncpy(upper_pair, pair->symbol, MAX_SYMBOL_LEN);
                    for (int k = 0; upper_pair[k]; k++) {
                        upper_pair[k] = toupper((unsigned char)upper_pair[k]);
                    }
                    
                    char upper_bot[MAX_SYMBOL_LEN];
                    strncpy(upper_bot, bot->symbol, MAX_SYMBOL_LEN);
                    for (int k = 0; upper_bot[k]; k++) {
                        upper_bot[k] = toupper((unsigned char)upper_bot[k]);
                    }
                    
                    if (strcmp(upper_pair, upper_bot) == 0) {
                        
                        if (pair->historical_5m_loaded && pair->historical_5m_count > 20) {
                            bot_process_signal(ctx->bot_manager, i, pair);
                        }
                        break;
                    }
                }
            }
        }
    }
    
    
    for (int i = 0; i < ctx->portfolio->pair_count; i++) {
        TradingPair *pair = &ctx->portfolio->pairs[i];
        time_t now = time(NULL);
        
        
        if (!pair->historical_1h_loaded || (now - pair->last_1h_fetch) > 300) {
            network_fetch_all_timeframes(ctx->network, pair->symbol, i, on_multi_timeframe_data, ctx);
        }
    }
}

static void on_theme_toggle_callback(void *user_data) {
    
    printf("Theme toggled\n");
}

static void on_show_optimization_callback(void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    
    PerformanceItem items[MAX_PAIRS];
    int item_count = 0;
    
    portfolio_analyze_performance(ctx->portfolio, items, &item_count);
    
    printf("\n=== Performance Analysis ===\n");
    for (int i = 0; i < item_count; i++) {
        printf("%s: %.2f%% (%.2f USD)\n", 
               items[i].symbol, 
               items[i].profit_loss_percent,
               items[i].profit_loss_value);
    }
}

static void on_show_opportunities_callback(void *user_data) {
    printf("Showing investment opportunities...\n");
    
}

int main(int argc, char *argv[]) {
    
    Portfolio *portfolio = portfolio_create();
    if (!portfolio) {
        fprintf(stderr, "Failed to create portfolio\n");
        return 1;
    }
    
    
    if (!portfolio_load(portfolio)) {
        printf("No saved portfolio found, initializing with defaults\n");
        portfolio_init_default(portfolio);
        portfolio_save(portfolio);
    }
    
    
    NetworkManager *network = network_manager_create();
    if (!network) {
        fprintf(stderr, "Failed to create network manager\n");
        portfolio_destroy(portfolio);
        return 1;
    }
    
    
    BotManager *bot_manager = bot_manager_create();
    if (!bot_manager) {
        fprintf(stderr, "Failed to create bot manager\n");
        network_manager_destroy(network);
        portfolio_destroy(portfolio);
        return 1;
    }
    
    
    bot_manager_load(bot_manager);
    
    
    AppContext ctx = {
        .portfolio = portfolio,
        .network = network,
        .bot_manager = bot_manager,
        .ui = NULL
    };
    
    
    UICallbacks callbacks = {
        .on_add_pair = on_add_pair_callback,
        .on_remove_pair = on_remove_pair_callback,
        .on_edit_pair = on_edit_pair_callback,
        .on_refresh = on_refresh_callback,
        .on_theme_toggle = on_theme_toggle_callback,
        .on_show_optimization = on_show_optimization_callback,
        .on_show_opportunities = on_show_opportunities_callback
    };
    
    
#if defined(__APPLE__)
    UIInterface *ui = ui_factory_create(UI_TYPE_COCOA, portfolio, network, &callbacks, &ctx);
#elif defined(_WIN32)
    UIInterface *ui = ui_factory_create(UI_TYPE_WINDOWS, portfolio, network, &callbacks, &ctx);
#else
    UIInterface *ui = ui_factory_create(UI_TYPE_GTK, portfolio, network, &callbacks, &ctx);
#endif
    
    if (!ui) {
        fprintf(stderr, "Failed to create UI\n");
        network_manager_destroy(network);
        portfolio_destroy(portfolio);
        return 1;
    }
    
    ctx.ui = ui;
    
    
    if (ui->init) {
        ui->init(argc, argv, ui->impl_data);
    }
    
    if (ui->run) {
        ui->run(ui->impl_data);
    }
    
    
    printf("Saving portfolio before exit...\n");
    portfolio_save(portfolio);
    
    printf("Saving bot manager state...\n");
    bot_manager_save(bot_manager);
    
    ui_factory_destroy(ui);
    bot_manager_destroy(bot_manager);
    network_manager_destroy(network);
    portfolio_destroy(portfolio);
    
    printf("Application closed successfully\n");
    return 0;
}
