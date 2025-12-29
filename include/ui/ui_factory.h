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

#ifndef UI_FACTORY_H
#define UI_FACTORY_H

#include "portfolio/portfolio_core.h"
#include "portfolio/network.h"


typedef struct {
    void (*on_add_pair)(const char *symbol, double bought_price, double quantity, 
                       PositionType position_type, void *user_data);
    void (*on_remove_pair)(int pair_index, void *user_data);
    void (*on_edit_pair)(int pair_index, const char *symbol, double bought_price, double quantity, 
                        PositionType position_type, void *user_data);
    void (*on_refresh)(void *user_data);
    void (*on_theme_toggle)(void *user_data);
    void (*on_show_optimization)(void *user_data);
    void (*on_show_opportunities)(void *user_data);
} UICallbacks;


typedef struct UIInterface {
    void (*init)(int argc, char *argv[], void *user_data);
    void (*run)(void *user_data);
    void (*update_portfolio_display)(Portfolio *portfolio, void *user_data);
    void (*update_pair_price)(int pair_index, double price, void *user_data);
    void (*show_error)(const char *message, void *user_data);
    void (*show_info)(const char *message, void *user_data);
    void (*cleanup)(void *user_data);
    void *impl_data; 
} UIInterface;


typedef enum {
    UI_TYPE_GTK,
    UI_TYPE_COCOA,
    UI_TYPE_WINDOWS,
    UI_TYPE_QT
} UIType;


UIInterface* ui_factory_create(UIType type, Portfolio *portfolio, NetworkManager *network,
                                 const UICallbacks *callbacks, void *user_data);
void ui_factory_destroy(UIInterface *ui);

#endif 
