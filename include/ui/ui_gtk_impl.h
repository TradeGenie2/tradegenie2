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

#ifndef UI_GTK_IMPL_H
#define UI_GTK_IMPL_H

#include "ui/ui_factory.h"
#include "portfolio/portfolio_core.h"
#include "portfolio/network.h"
#include "portfolio/scalping_bot.h"
#include <gtk/gtk.h>


typedef struct {
    Portfolio *portfolio;
    NetworkManager *network;
    BotManager *bot_manager;
    UICallbacks callbacks;
    void *user_data;
    
    
    GtkWidget *window;
    GtkWidget *portfolio_box;
    GtkWidget *total_label;
    GtkWidget *theme_button;
    
    
    guint timer_id;
    gboolean dragging;
    gboolean resizing;
    gint drag_start_x;
    gint drag_start_y;
    gint resize_edge;
    
    
    gboolean is_sliding;
    gboolean is_visible;
    guint animation_timer;
    gint target_x;
    gint current_x;
    gint screen_width;
    gint window_width;
    
    
    gboolean dark_theme;
} GTKAppData;


UIInterface* ui_gtk_create(Portfolio *portfolio, NetworkManager *network,
                           const UICallbacks *callbacks, void *user_data);
void ui_gtk_destroy(UIInterface *ui);

#endif 
