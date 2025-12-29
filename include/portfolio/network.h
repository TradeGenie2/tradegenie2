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

#ifndef PORTFOLIO_NETWORK_H
#define PORTFOLIO_NETWORK_H

#include "portfolio_core.h"
#include <libsoup/soup.h>


typedef void (*PriceUpdateCallback)(int pair_index, double price, void *user_data);
typedef void (*HistoricalDataCallback)(int pair_index, double *prices, int count, void *user_data);
typedef void (*MultiTimeframeCallback)(int pair_index, const char *interval, double *prices, int count, void *user_data);


typedef struct NetworkManager {
    SoupSession *session;
} NetworkManager;


NetworkManager* network_manager_create(void);
void network_manager_destroy(NetworkManager *manager);


void network_fetch_price(NetworkManager *manager, const char *symbol, int pair_index, 
                         PriceUpdateCallback callback, void *user_data);
void network_fetch_historical(NetworkManager *manager, const char *symbol, int pair_index,
                               HistoricalDataCallback callback, void *user_data);
void network_fetch_all_prices(NetworkManager *manager, Portfolio *portfolio,
                               PriceUpdateCallback callback, void *user_data);


void network_fetch_timeframe(NetworkManager *manager, const char *symbol, int pair_index,
                             const char *interval, int limit, 
                             MultiTimeframeCallback callback, void *user_data);
void network_fetch_all_timeframes(NetworkManager *manager, const char *symbol, int pair_index,
                                  MultiTimeframeCallback callback, void *user_data);

#endif 
