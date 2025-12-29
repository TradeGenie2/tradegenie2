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

#include "portfolio/network.h"
#include <json-c/json.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    NetworkManager *manager;
    int pair_index;
    PriceUpdateCallback callback;
    void *user_data;
} PriceCallbackData;

typedef struct {
    NetworkManager *manager;
    int pair_index;
    HistoricalDataCallback callback;
    void *user_data;
} HistoricalCallbackData;

typedef struct {
    NetworkManager *manager;
    int pair_index;
    MultiTimeframeCallback callback;
    void *user_data;
    char interval[8];
} MultiTimeframeCallbackData;

NetworkManager* network_manager_create(void) {
    NetworkManager *manager = malloc(sizeof(NetworkManager));
    if (!manager) return NULL;
    
    manager->session = soup_session_new();
    return manager;
}

void network_manager_destroy(NetworkManager *manager) {
    if (!manager) return;
    
    if (manager->session) {
        g_object_unref(manager->session);
    }
    free(manager);
}

static void price_fetch_callback(SoupSession *session, SoupMessage *msg, gpointer user_data) {
    PriceCallbackData *data = (PriceCallbackData *)user_data;
    
    if (msg->status_code != 200) {
        fprintf(stderr, "Failed to fetch price: HTTP %u\n", msg->status_code);
        free(data);
        return;
    }
    
    const char *response_body = msg->response_body->data;
    struct json_object *root = json_tokener_parse(response_body);
    
    if (!root) {
        fprintf(stderr, "Failed to parse price JSON\n");
        free(data);
        return;
    }
    
    struct json_object *price_obj;
    if (json_object_object_get_ex(root, "price", &price_obj)) {
        const char *price_str = json_object_get_string(price_obj);
        double price = atof(price_str);
        
        if (data->callback) {
            data->callback(data->pair_index, price, data->user_data);
        }
    }
    
    json_object_put(root);
    free(data);
}

void network_fetch_price(NetworkManager *manager, const char *symbol, int pair_index,
                         PriceUpdateCallback callback, void *user_data) {
    if (!manager || !symbol) return;
    
    
    char upper_symbol[MAX_SYMBOL_LEN];
    strncpy(upper_symbol, symbol, MAX_SYMBOL_LEN - 1);
    upper_symbol[MAX_SYMBOL_LEN - 1] = '\0';
    for (int i = 0; upper_symbol[i]; i++) {
        upper_symbol[i] = toupper((unsigned char)upper_symbol[i]);
    }
    
    char url[256];
    snprintf(url, sizeof(url), "https://api.binance.com/api/v3/ticker/price?symbol=%s", upper_symbol);
    
    SoupMessage *msg = soup_message_new("GET", url);
    
    PriceCallbackData *data = malloc(sizeof(PriceCallbackData));
    data->manager = manager;
    data->pair_index = pair_index;
    data->callback = callback;
    data->user_data = user_data;
    
    soup_session_queue_message(manager->session, msg, price_fetch_callback, data);
}

static void historical_fetch_callback(SoupSession *session, SoupMessage *msg, gpointer user_data) {
    HistoricalCallbackData *data = (HistoricalCallbackData *)user_data;
    
    if (msg->status_code != 200) {
        fprintf(stderr, "Failed to fetch historical data: HTTP %u\n", msg->status_code);
        free(data);
        return;
    }
    
    const char *response_body = msg->response_body->data;
    struct json_object *root = json_tokener_parse(response_body);
    
    if (!root || json_object_get_type(root) != json_type_array) {
        fprintf(stderr, "Failed to parse historical JSON\n");
        if (root) json_object_put(root);
        free(data);
        return;
    }
    
    int array_len = json_object_array_length(root);
    if (array_len > HISTORICAL_DATA_SIZE) {
        array_len = HISTORICAL_DATA_SIZE;
    }
    
    double *prices = malloc(array_len * sizeof(double));
    int count = 0;
    
    for (int i = 0; i < array_len; i++) {
        struct json_object *candle = json_object_array_get_idx(root, i);
        if (json_object_get_type(candle) == json_type_array) {
            struct json_object *close_price_obj = json_object_array_get_idx(candle, 4);
            if (close_price_obj) {
                const char *close_str = json_object_get_string(close_price_obj);
                prices[count++] = atof(close_str);
            }
        }
    }
    
    if (count > 0 && data->callback) {
        data->callback(data->pair_index, prices, count, data->user_data);
    }
    
    free(prices);
    json_object_put(root);
    free(data);
}

void network_fetch_historical(NetworkManager *manager, const char *symbol, int pair_index,
                               HistoricalDataCallback callback, void *user_data) {
    if (!manager || !symbol) return;
    
    
    char upper_symbol[MAX_SYMBOL_LEN];
    strncpy(upper_symbol, symbol, MAX_SYMBOL_LEN - 1);
    upper_symbol[MAX_SYMBOL_LEN - 1] = '\0';
    for (int i = 0; upper_symbol[i]; i++) {
        upper_symbol[i] = toupper((unsigned char)upper_symbol[i]);
    }
    
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.binance.com/api/v3/klines?symbol=%s&interval=1h&limit=100",
             upper_symbol);
    
    SoupMessage *msg = soup_message_new("GET", url);
    
    HistoricalCallbackData *data = malloc(sizeof(HistoricalCallbackData));
    data->manager = manager;
    data->pair_index = pair_index;
    data->callback = callback;
    data->user_data = user_data;
    
    soup_session_queue_message(manager->session, msg, historical_fetch_callback, data);
}

void network_fetch_all_prices(NetworkManager *manager, Portfolio *portfolio,
                               PriceUpdateCallback callback, void *user_data) {
    if (!manager || !portfolio) return;
    
    for (int i = 0; i < portfolio->pair_count; i++) {
        network_fetch_price(manager, portfolio->pairs[i].symbol, i, callback, user_data);
    }
}


static void timeframe_fetch_callback(SoupSession *session, SoupMessage *msg, gpointer user_data) {
    MultiTimeframeCallbackData *data = (MultiTimeframeCallbackData *)user_data;
    
    if (msg->status_code != 200) {
        fprintf(stderr, "Failed to fetch %s data: HTTP %u\n", data->interval, msg->status_code);
        free(data);
        return;
    }
    
    const char *response_body = msg->response_body->data;
    struct json_object *root = json_tokener_parse(response_body);
    
    if (!root || json_object_get_type(root) != json_type_array) {
        fprintf(stderr, "Failed to parse %s JSON\n", data->interval);
        if (root) json_object_put(root);
        free(data);
        return;
    }
    
    int array_len = json_object_array_length(root);
    int max_len = HISTORICAL_DATA_SIZE_1H;  
    if (array_len > max_len) {
        array_len = max_len;
    }
    
    double *prices = malloc(array_len * sizeof(double));
    int count = 0;
    
    for (int i = 0; i < array_len; i++) {
        struct json_object *candle = json_object_array_get_idx(root, i);
        if (json_object_get_type(candle) == json_type_array) {
            struct json_object *close_price_obj = json_object_array_get_idx(candle, 4);
            if (close_price_obj) {
                const char *close_str = json_object_get_string(close_price_obj);
                prices[count++] = atof(close_str);
            }
        }
    }
    
    if (count > 0 && data->callback) {
        data->callback(data->pair_index, data->interval, prices, count, data->user_data);
    }
    
    free(prices);
    json_object_put(root);
    free(data);
}


void network_fetch_timeframe(NetworkManager *manager, const char *symbol, int pair_index,
                             const char *interval, int limit, 
                             MultiTimeframeCallback callback, void *user_data) {
    if (!manager || !symbol || !interval) return;
    
    
    char upper_symbol[MAX_SYMBOL_LEN];
    strncpy(upper_symbol, symbol, MAX_SYMBOL_LEN - 1);
    upper_symbol[MAX_SYMBOL_LEN - 1] = '\0';
    for (int i = 0; upper_symbol[i]; i++) {
        upper_symbol[i] = toupper((unsigned char)upper_symbol[i]);
    }
    
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.binance.com/api/v3/klines?symbol=%s&interval=%s&limit=%d",
             upper_symbol, interval, limit);
    
    SoupMessage *msg = soup_message_new("GET", url);
    
    MultiTimeframeCallbackData *data = malloc(sizeof(MultiTimeframeCallbackData));
    data->manager = manager;
    data->pair_index = pair_index;
    data->callback = callback;
    data->user_data = user_data;
    strncpy(data->interval, interval, sizeof(data->interval) - 1);
    data->interval[sizeof(data->interval) - 1] = '\0';
    
    soup_session_queue_message(manager->session, msg, timeframe_fetch_callback, data);
}


void network_fetch_all_timeframes(NetworkManager *manager, const char *symbol, int pair_index,
                                  MultiTimeframeCallback callback, void *user_data) {
    if (!manager || !symbol) return;
    
    
    network_fetch_timeframe(manager, symbol, pair_index, "5m", 288, callback, user_data);
    network_fetch_timeframe(manager, symbol, pair_index, "15m", 192, callback, user_data);
    
    
    network_fetch_timeframe(manager, symbol, pair_index, "1h", 500, callback, user_data);
    network_fetch_timeframe(manager, symbol, pair_index, "4h", 200, callback, user_data);
    
    
    network_fetch_timeframe(manager, symbol, pair_index, "1d", 100, callback, user_data);
}
