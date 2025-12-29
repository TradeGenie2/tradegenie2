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

#include "ui/ui_gtk_impl.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <json-c/json.h>


typedef struct AppContext AppContext;
struct AppContext {
    Portfolio *portfolio;
    NetworkManager *network;
    BotManager *bot_manager;
    UIInterface *ui;
};


static void gtk_ui_init(int argc, char *argv[], void *user_data);
static void gtk_ui_run(void *user_data);
static void gtk_ui_update_portfolio_display(Portfolio *portfolio, void *user_data);
static void gtk_ui_update_pair_price(int pair_index, double price, void *user_data);
static void gtk_ui_show_error(const char *message, void *user_data);
static void gtk_ui_show_info(const char *message, void *user_data);
static void gtk_ui_cleanup(void *user_data);


static void apply_css_styling(GTKAppData *app);
static void apply_theme(GTKAppData *app);
static void update_display(GTKAppData *app);
static GtkWidget* create_sparkline_widget(TradingPair *pair);
static gboolean draw_sparkline(GtkWidget *widget, cairo_t *cr, gpointer user_data);


static void on_add_pair_clicked(GtkButton *button, gpointer user_data);
static void on_refresh_clicked(GtkButton *button, gpointer user_data);
static void on_theme_toggle_clicked(GtkButton *button, gpointer user_data);
static void on_remove_pair_clicked(GtkButton *button, gpointer user_data);
static void on_rebalance_clicked(GtkButton *button, gpointer user_data);
static void on_opportunities_clicked(GtkButton *button, gpointer user_data);
static void on_bots_clicked(GtkButton *button, gpointer user_data);
static void on_toggle_border_clicked(GtkButton *button, gpointer user_data);
static gboolean on_card_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void on_window_destroy(GtkWidget *widget, gpointer user_data);
static gboolean timer_callback(gpointer user_data);


static void show_edit_pair_dialog(GTKAppData *app, int pair_index);
static void show_optimization_dialog(GTKAppData *app);
static void show_opportunities_dialog(GTKAppData *app);
static void show_bots_dialog(GTKAppData *app);

UIInterface* ui_gtk_create(Portfolio *portfolio, NetworkManager *network,
                           const UICallbacks *callbacks, void *user_data) {
    UIInterface *ui = malloc(sizeof(UIInterface));
    if (!ui) return NULL;
    
    GTKAppData *app_data = malloc(sizeof(GTKAppData));
    if (!app_data) {
        free(ui);
        return NULL;
    }
    
    memset(app_data, 0, sizeof(GTKAppData));
    app_data->portfolio = portfolio;
    app_data->network = network;
    
    
    if (user_data) {
        AppContext *ctx = (AppContext *)user_data;
        app_data->bot_manager = ctx->bot_manager;
    }
    
    if (callbacks) {
        app_data->callbacks = *callbacks;
    }
    app_data->user_data = user_data;
    app_data->dark_theme = false;
    
    ui->init = gtk_ui_init;
    ui->run = gtk_ui_run;
    ui->update_portfolio_display = gtk_ui_update_portfolio_display;
    ui->update_pair_price = gtk_ui_update_pair_price;
    ui->show_error = gtk_ui_show_error;
    ui->show_info = gtk_ui_show_info;
    ui->cleanup = gtk_ui_cleanup;
    ui->impl_data = app_data;
    
    return ui;
}

void ui_gtk_destroy(UIInterface *ui) {
    if (!ui) return;
    
    if (ui->impl_data) {
        GTKAppData *app_data = (GTKAppData *)ui->impl_data;
        
        if (app_data->timer_id > 0) {
            g_source_remove(app_data->timer_id);
        }
        if (app_data->animation_timer > 0) {
            g_source_remove(app_data->animation_timer);
        }
        
        free(app_data);
    }
    
    free(ui);
}

static void gtk_ui_init(int argc, char *argv[], void *user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    gtk_init(&argc, &argv);
    
    
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "Crypto Portfolio");
    gtk_window_set_resizable(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 400, 600);
    
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_window_destroy), app);
    
    
    g_object_set_data(G_OBJECT(app->window), "app_data", app);
    
    
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(main_box, 16);
    gtk_widget_set_margin_end(main_box, 16);
    gtk_widget_set_margin_top(main_box, 16);
    gtk_widget_set_margin_bottom(main_box, 16);
    
    
    GtkWidget *title_label = gtk_label_new("Portfolio");
    gtk_widget_set_name(title_label, "title-label");
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), title_label, FALSE, FALSE, 0);
    
    
    app->total_label = gtk_label_new("");
    gtk_widget_set_name(app->total_label, "total-label");
    gtk_widget_set_halign(app->total_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), app->total_label, FALSE, FALSE, 0);
    
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    
    app->portfolio_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_hexpand(app->portfolio_box, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), app->portfolio_box);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled, TRUE, TRUE, 0);
    
    
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    
    GtkWidget *add_button = gtk_button_new_with_label("+");
    gtk_widget_set_tooltip_text(add_button, "Add Crypto");
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_pair_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), add_button, FALSE, FALSE, 0);
    
    GtkWidget *refresh_button = gtk_button_new_with_label("⟳");
    gtk_widget_set_tooltip_text(refresh_button, "Refresh Prices");
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), refresh_button, FALSE, FALSE, 0);
    
    GtkWidget *border_button = gtk_button_new_with_label("☐");
    gtk_widget_set_tooltip_text(border_button, "Show Window Controls");
    g_signal_connect(border_button, "clicked", G_CALLBACK(on_toggle_border_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), border_button, FALSE, FALSE, 0);
    
    app->theme_button = gtk_button_new_with_label("☾");
    gtk_widget_set_tooltip_text(app->theme_button, "Switch to Dark Theme");
    g_signal_connect(app->theme_button, "clicked", G_CALLBACK(on_theme_toggle_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), app->theme_button, FALSE, FALSE, 0);
    
    GtkWidget *rebalance_button = gtk_button_new_with_label("⚡");
    gtk_widget_set_tooltip_text(rebalance_button, "Growth Opportunities");
    g_signal_connect(rebalance_button, "clicked", G_CALLBACK(on_rebalance_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), rebalance_button, FALSE, FALSE, 0);
    
    GtkWidget *opportunities_button = gtk_button_new_with_label("◎");
    gtk_widget_set_tooltip_text(opportunities_button, "Discover New Assets");
    g_signal_connect(opportunities_button, "clicked", G_CALLBACK(on_opportunities_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), opportunities_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(main_container), main_box);
    gtk_container_add(GTK_CONTAINER(app->window), main_container);
    
    
    apply_css_styling(app);
    
    
    gtk_widget_show_all(app->window);
    
    
    apply_theme(app);
    
    
    app->timer_id = g_timeout_add_seconds(5, timer_callback, app);
}

static void gtk_ui_run(void *user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    
    update_display(app);
    
    
    if (app->callbacks.on_refresh) {
        app->callbacks.on_refresh(app->user_data);
    }
    
    printf("GTK Crypto Portfolio Widget started!\n");
    
    gtk_main();
}

static void gtk_ui_cleanup(void *user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
    }
    if (app->animation_timer > 0) {
        g_source_remove(app->animation_timer);
        app->animation_timer = 0;
    }
}

static gboolean draw_sparkline(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    TradingPair *pair = (TradingPair *)user_data;
    
    if (pair->history_count < 2) {
        return FALSE;
    }
    
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    int width = allocation.width;
    int height = allocation.height;
    int margin = 2;
    
    double min_price = pair->price_history[0];
    double max_price = pair->price_history[0];
    
    for (int i = 0; i < pair->history_count; i++) {
        double price = pair->price_history[i];
        if (price < min_price) min_price = price;
        if (price > max_price) max_price = price;
    }
    
    if (max_price == min_price) {
        max_price = min_price + 1.0;
    }
    
    cairo_set_line_width(cr, 1.5);
    
    double first_price = pair->price_history[0];
    double last_price = pair->price_history[(pair->history_index - 1 + PRICE_HISTORY_SIZE) % PRICE_HISTORY_SIZE];
    
    if (last_price >= first_price) {
        cairo_set_source_rgba(cr, 0.188, 0.820, 0.341, 0.9);
    } else {
        cairo_set_source_rgba(cr, 1.0, 0.271, 0.227, 0.9);
    }
    
    int display_count = pair->history_count;
    for (int i = 0; i < display_count; i++) {
        int idx = (pair->history_index - display_count + i + PRICE_HISTORY_SIZE) % PRICE_HISTORY_SIZE;
        double price = pair->price_history[idx];
        
        double x = margin + ((double)i / (display_count - 1)) * (width - 2 * margin);
        double normalized = (price - min_price) / (max_price - min_price);
        double y = height - margin - (normalized * (height - 2 * margin));
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    
    cairo_stroke(cr);
    return FALSE;
}

static GtkWidget* create_sparkline_widget(TradingPair *pair) {
    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 100, 30);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(draw_sparkline), pair);
    return drawing_area;
}

static void update_display(GTKAppData *app) {
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->portfolio_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    
    double total_value = portfolio_get_total_value(app->portfolio);
    double total_cost = portfolio_get_total_cost(app->portfolio);
    double total_pl = portfolio_get_total_profit_loss(app->portfolio);
    double total_pl_pct = portfolio_get_total_profit_loss_percent(app->portfolio);
    
    
    char total_text[256];
    snprintf(total_text, sizeof(total_text), 
             "Total: $%.2f (%.2f%%)", total_value, total_pl_pct);
    gtk_label_set_text(GTK_LABEL(app->total_label), total_text);
    
    
    for (int i = 0; i < app->portfolio->pair_count; i++) {
        TradingPair *pair = &app->portfolio->pairs[i];
        
        GtkWidget *card = gtk_event_box_new();
        gtk_widget_set_name(card, "portfolio-card");
        
        
        int *pair_index = g_malloc(sizeof(int));
        *pair_index = i;
        g_signal_connect(card, "button-press-event", 
                        G_CALLBACK(on_card_button_press), pair_index);
        
        GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start(card_box, 12);
        gtk_widget_set_margin_end(card_box, 12);
        gtk_widget_set_margin_top(card_box, 12);
        gtk_widget_set_margin_bottom(card_box, 12);
        
        
        GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        
        GtkWidget *symbol_label = gtk_label_new(pair->symbol);
        gtk_widget_set_name(symbol_label, "symbol-label");
        gtk_widget_set_halign(symbol_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand(symbol_label, TRUE);
        gtk_box_pack_start(GTK_BOX(header_box), symbol_label, TRUE, TRUE, 0);
        
        
        GtkWidget *remove_btn = gtk_button_new_with_label("×");
        gtk_widget_set_name(remove_btn, "remove-button");
        int *remove_index = g_malloc(sizeof(int));
        *remove_index = i;
        g_signal_connect(remove_btn, "clicked", 
                        G_CALLBACK(on_remove_pair_clicked), remove_index);
        gtk_box_pack_end(GTK_BOX(header_box), remove_btn, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(card_box), header_box, FALSE, FALSE, 0);
        
        
        if (pair->current_price > 0) {
            char price_text[128];
            snprintf(price_text, sizeof(price_text), "$%.2f", pair->current_price);
            GtkWidget *price_label = gtk_label_new(price_text);
            gtk_widget_set_name(price_label, "price-label");
            gtk_widget_set_halign(price_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(card_box), price_label, FALSE, FALSE, 0);
        }
        
        
        if (pair->history_count > 0) {
            GtkWidget *sparkline = create_sparkline_widget(pair);
            gtk_box_pack_start(GTK_BOX(card_box), sparkline, FALSE, FALSE, 0);
        }
        
        
        char holdings_text[256];
        double entry_value = pair->bought_price * pair->quantity;
        double current_value = pair->current_price * pair->quantity;
        double pl, pl_pct;
        
        const char *position_label = (pair->position_type == POSITION_LONG) ? "LONG" : "SHORT";
        const char *position_color = (pair->position_type == POSITION_LONG) ? "#007AFF" : "#FF9500";
        
        if (pair->position_type == POSITION_LONG) {
            pl = current_value - entry_value;
            pl_pct = (entry_value > 0) ? (pl / entry_value) * 100.0 : 0.0;
            current_value = pair->current_price * pair->quantity;
        } else {
            
            pl = entry_value - current_value;
            pl_pct = (entry_value > 0) ? (pl / entry_value) * 100.0 : 0.0;
            current_value = entry_value + pl;
        }
        
        snprintf(holdings_text, sizeof(holdings_text),
                 "<span foreground='%s'><b>%s</b></span> | %.4f @ $%.2f | Value: $%.2f", 
                 position_color, position_label, pair->quantity, pair->bought_price, current_value);
        
        GtkWidget *holdings_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(holdings_label), holdings_text);
        gtk_widget_set_name(holdings_label, "holdings-label");
        gtk_widget_set_halign(holdings_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), holdings_label, FALSE, FALSE, 0);
        
        
        GtkWidget *pl_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        
        char pl_text[128];
        snprintf(pl_text, sizeof(pl_text), 
                 "%s$%.2f (%.2f%%)",
                 pl >= 0 ? "+" : "", pl, pl_pct);
        GtkWidget *pl_label = gtk_label_new(pl_text);
        gtk_widget_set_name(pl_label, pl >= 0 ? "profit-label" : "loss-label");
        gtk_widget_set_halign(pl_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(pl_row), pl_label, FALSE, FALSE, 0);
        
        
        int trend = portfolio_calculate_trend(pair);
        double momentum = portfolio_calculate_momentum(pair);
        const char *rec_text = portfolio_get_recommendation_text(pair, pl_pct, trend, momentum);
        const char *rec_color = portfolio_get_recommendation_color(pair, pl_pct, trend, momentum);
        
        GtkWidget *rec_label = gtk_label_new(NULL);
        char rec_markup[256];
        snprintf(rec_markup, sizeof(rec_markup),
                "<span size='small' foreground='%s'><b>%s</b></span>",
                rec_color, rec_text);
        gtk_label_set_markup(GTK_LABEL(rec_label), rec_markup);
        gtk_widget_set_halign(rec_label, GTK_ALIGN_END);
        gtk_widget_set_hexpand(rec_label, TRUE);
        gtk_box_pack_end(GTK_BOX(pl_row), rec_label, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(card_box), pl_row, FALSE, FALSE, 0);
        
        
        if ((pair->historical_loaded && pair->historical_count > 20) || 
            (pair->historical_1h_loaded && pair->historical_1h_count > 20)) {
            double buy_price = 0.0, sell_price = 0.0;
            char buy_reason[128] = "", sell_reason[128] = "";
            portfolio_calculate_trade_prices(pair, &buy_price, &sell_price, buy_reason, sell_reason);
            
            
            TargetAnalysis sell_analysis, buy_analysis;
            portfolio_analyze_target(pair, sell_price, true, &sell_analysis);
            portfolio_analyze_target(pair, buy_price, false, &buy_analysis);
            
            GtkWidget *ta_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_widget_set_margin_top(ta_box, 4);
            
            
            GtkWidget *sell_label = gtk_label_new(NULL);
            char sell_markup[512];
            double sell_diff = ((sell_price - pair->current_price) / pair->current_price) * 100.0;
            const char *sell_color = sell_price > pair->current_price ? "#30d158" : "#ff9500";
            
            
            char time_str[64];
            if (sell_analysis.estimated_hours < 24) {
                snprintf(time_str, sizeof(time_str), "%dh", sell_analysis.estimated_hours);
            } else {
                snprintf(time_str, sizeof(time_str), "%dd", sell_analysis.estimated_hours / 24);
            }
            
            snprintf(sell_markup, sizeof(sell_markup),
                    "<span size='small'><span foreground='%s'><b>$%.6f</b></span> (+%.1f%%) - %s\n"
                    "  <span foreground='#8e8e93' size='x-small'>Expected: $%.2f (%.1f%%) • %.0f%% prob • ~%s • %s</span></span>",
                    sell_color, sell_price, sell_diff, sell_reason,
                    sell_analysis.expected_profit_loss, sell_analysis.expected_profit_pct,
                    sell_analysis.probability * 100.0, time_str, sell_analysis.confidence_level);
            gtk_label_set_markup(GTK_LABEL(sell_label), sell_markup);
            gtk_widget_set_halign(sell_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(ta_box), sell_label, FALSE, FALSE, 0);
            
            
            GtkWidget *buy_label = gtk_label_new(NULL);
            char buy_markup[512];
            double buy_diff = ((buy_price - pair->current_price) / pair->current_price) * 100.0;
            const char *buy_color = buy_price < pair->current_price ? "#30d158" : "#ff9500";
            
            
            if (buy_analysis.estimated_hours < 24) {
                snprintf(time_str, sizeof(time_str), "%dh", buy_analysis.estimated_hours);
            } else {
                snprintf(time_str, sizeof(time_str), "%dd", buy_analysis.estimated_hours / 24);
            }
            
            snprintf(buy_markup, sizeof(buy_markup),
                    "<span size='small'><span foreground='%s'><b>$%.6f</b></span> (%.1f%%) - %s\n"
                    "  <span foreground='#8e8e93' size='x-small'>%.0f%% prob • ~%s • %s</span></span>",
                    buy_color, buy_price, buy_diff, buy_reason,
                    buy_analysis.probability * 100.0, time_str, buy_analysis.confidence_level);
            gtk_label_set_markup(GTK_LABEL(buy_label), buy_markup);
            gtk_widget_set_halign(buy_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(ta_box), buy_label, FALSE, FALSE, 0);
            
            gtk_box_pack_start(GTK_BOX(card_box), ta_box, FALSE, FALSE, 0);
        }
        
        
        if (pair->historical_1h_loaded || pair->historical_loaded) {
            GtkWidget *enhanced_ta_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_widget_set_margin_top(enhanced_ta_box, 4);
            
            
            double prob_pct = pair->profit_probability * 100.0;
            const char *prob_color;
            const char *prob_confidence;
            if (prob_pct >= 70) {
                prob_color = "#30d158";
                prob_confidence = "HIGH";
            } else if (prob_pct >= 55) {
                prob_color = "#ffd60a";
                prob_confidence = "MEDIUM";
            } else if (prob_pct >= 45) {
                prob_color = "#8e8e93";
                prob_confidence = "NEUTRAL";
            } else if (prob_pct >= 30) {
                prob_color = "#ff9500";
                prob_confidence = "LOW";
            } else {
                prob_color = "#ff453a";
                prob_confidence = "VERY LOW";
            }
            
            GtkWidget *prob_label = gtk_label_new(NULL);
            char prob_markup[256];
            snprintf(prob_markup, sizeof(prob_markup),
                    "<span size='small'>[P] Profit Probability: <span foreground='%s'><b>%.1f%%</b></span> (%s)</span>",
                    prob_color, prob_pct, prob_confidence);
            gtk_label_set_markup(GTK_LABEL(prob_label), prob_markup);
            gtk_widget_set_halign(prob_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(enhanced_ta_box), prob_label, FALSE, FALSE, 0);
            
            
            if (pair->pattern_count > 0 && strlen(pair->detected_patterns) > 0) {
                GtkWidget *pattern_label = gtk_label_new(NULL);
                char pattern_markup[400];
                snprintf(pattern_markup, sizeof(pattern_markup),
                        "<span size='small'>[*] Patterns: <span foreground='#bf5af2'><b>%s</b></span></span>",
                        pair->detected_patterns);
                gtk_label_set_markup(GTK_LABEL(pattern_label), pattern_markup);
                gtk_widget_set_halign(pattern_label, GTK_ALIGN_START);
                gtk_box_pack_start(GTK_BOX(enhanced_ta_box), pattern_label, FALSE, FALSE, 0);
            }
            
            
            if (pair->historical_1h_loaded && pair->historical_4h_loaded && pair->historical_1d_loaded) {
                int mtf_trend = calculate_trend_multi_timeframe(pair);
                const char *trend_arrow_1h = "→";
                const char *trend_arrow_4h = "→";
                const char *trend_arrow_1d = "→";
                
                
                if (pair->historical_1h_count >= 20) {
                    double avg_early = 0.0, avg_recent = 0.0;
                    for (int j = pair->historical_1h_count - 20; j < pair->historical_1h_count - 10; j++) {
                        if (j >= 0) avg_early += pair->historical_1h[j];
                    }
                    for (int j = pair->historical_1h_count - 10; j < pair->historical_1h_count; j++) {
                        if (j >= 0) avg_recent += pair->historical_1h[j];
                    }
                    trend_arrow_1h = (avg_recent > avg_early * 1.01) ? "↗" : (avg_recent < avg_early * 0.99) ? "↘" : "→";
                }
                
                if (pair->historical_4h_count >= 20) {
                    double avg_early = 0.0, avg_recent = 0.0;
                    for (int j = pair->historical_4h_count - 20; j < pair->historical_4h_count - 10; j++) {
                        if (j >= 0) avg_early += pair->historical_4h[j];
                    }
                    for (int j = pair->historical_4h_count - 10; j < pair->historical_4h_count; j++) {
                        if (j >= 0) avg_recent += pair->historical_4h[j];
                    }
                    trend_arrow_4h = (avg_recent > avg_early * 1.01) ? "↗" : (avg_recent < avg_early * 0.99) ? "↘" : "→";
                }
                
                if (pair->historical_1d_count >= 20) {
                    double avg_early = 0.0, avg_recent = 0.0;
                    for (int j = pair->historical_1d_count - 20; j < pair->historical_1d_count - 10; j++) {
                        if (j >= 0) avg_early += pair->historical_1d[j];
                    }
                    for (int j = pair->historical_1d_count - 10; j < pair->historical_1d_count; j++) {
                        if (j >= 0) avg_recent += pair->historical_1d[j];
                    }
                    trend_arrow_1d = (avg_recent > avg_early * 1.01) ? "↗" : (avg_recent < avg_early * 0.99) ? "↘" : "→";
                }
                
                const char *alignment_text = mtf_trend == 1 ? "All Bullish!" : 
                                            mtf_trend == -1 ? "All Bearish" : 
                                            "Mixed";
                const char *alignment_color = mtf_trend == 1 ? "#30d158" : 
                                              mtf_trend == -1 ? "#ff453a" : 
                                              "#8e8e93";
                
                GtkWidget *trend_label = gtk_label_new(NULL);
                char trend_markup[400];
                snprintf(trend_markup, sizeof(trend_markup),
                        "<span size='small'>[T] Trends: 1h%s 4h%s 1d%s - <span foreground='%s'><b>%s</b></span></span>",
                        trend_arrow_1h, trend_arrow_4h, trend_arrow_1d, alignment_color, alignment_text);
                gtk_label_set_markup(GTK_LABEL(trend_label), trend_markup);
                gtk_widget_set_halign(trend_label, GTK_ALIGN_START);
                gtk_box_pack_start(GTK_BOX(enhanced_ta_box), trend_label, FALSE, FALSE, 0);
            }
            
            
            if (pair->historical_5m_loaded && strlen(pair->scalp_signal) > 0) {
                const char *signal_color;
                if (strstr(pair->scalp_signal, "BUY")) {
                    signal_color = "#30d158";
                } else if (strstr(pair->scalp_signal, "SELL")) {
                    signal_color = "#ff453a";
                } else if (strstr(pair->scalp_signal, "HOLD")) {
                    signal_color = "#ffd60a";
                } else if (strstr(pair->scalp_signal, "WAIT")) {
                    signal_color = "#8e8e93";
                } else {
                    signal_color = "#ff9500";
                }
                
                GtkWidget *scalp_label = gtk_label_new(NULL);
                char scalp_markup[256];
                snprintf(scalp_markup, sizeof(scalp_markup),
                        "<span size='small'>[S] <b>Scalp Signal:</b> <span foreground='%s'><b>%s</b></span></span>",
                        signal_color, pair->scalp_signal);
                gtk_label_set_markup(GTK_LABEL(scalp_label), scalp_markup);
                gtk_widget_set_halign(scalp_label, GTK_ALIGN_START);
                gtk_box_pack_start(GTK_BOX(enhanced_ta_box), scalp_label, FALSE, FALSE, 0);
            }
            
            gtk_box_pack_start(GTK_BOX(card_box), enhanced_ta_box, FALSE, FALSE, 0);
        }
        
        gtk_container_add(GTK_CONTAINER(card), card_box);
        gtk_box_pack_start(GTK_BOX(app->portfolio_box), card, FALSE, FALSE, 0);
    }
    
    gtk_widget_show_all(app->portfolio_box);
}

static void gtk_ui_update_portfolio_display(Portfolio *portfolio, void *user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    update_display(app);
}

static void gtk_ui_update_pair_price(int pair_index, double price, void *user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    update_display(app);
}

static void gtk_ui_show_error(const char *message, void *user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void gtk_ui_show_info(const char *message, void *user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}


static void on_add_pair_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Add Trading Pair / Position",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Add", GTK_RESPONSE_OK,
        NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 12);
    gtk_widget_set_margin_bottom(grid, 12);
    
    GtkWidget *symbol_label = gtk_label_new("Symbol:");
    GtkWidget *symbol_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(symbol_entry), "e.g., btcusdt");
    
    GtkWidget *price_label = gtk_label_new("Entry Price:");
    GtkWidget *price_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(price_entry), "e.g., 30000.00");
    
    GtkWidget *quantity_label = gtk_label_new("Quantity:");
    GtkWidget *quantity_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(quantity_entry), "e.g., 2.5");
    
    
    GtkWidget *type_label = gtk_label_new("Position Type:");
    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *long_radio = gtk_radio_button_new_with_label(NULL, "LONG (Buy)");
    GtkWidget *short_radio = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(long_radio), "SHORT (Sell)");
    gtk_box_pack_start(GTK_BOX(type_box), long_radio, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(type_box), short_radio, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(long_radio), TRUE);
    
    gtk_grid_attach(GTK_GRID(grid), symbol_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), symbol_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), price_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), price_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quantity_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quantity_entry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), type_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), type_box, 1, 3, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        const char *symbol = gtk_entry_get_text(GTK_ENTRY(symbol_entry));
        const char *price_str = gtk_entry_get_text(GTK_ENTRY(price_entry));
        const char *qty_str = gtk_entry_get_text(GTK_ENTRY(quantity_entry));
        gboolean is_long = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(long_radio));
        
        if (strlen(symbol) > 0 && strlen(price_str) > 0 && strlen(qty_str) > 0) {
            double price = atof(price_str);
            double quantity = atof(qty_str);
            PositionType pos_type = is_long ? POSITION_LONG : POSITION_SHORT;
            
            if (app->callbacks.on_add_pair) {
                app->callbacks.on_add_pair(symbol, price, quantity, pos_type, app->user_data);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

static void on_remove_pair_clicked(GtkButton *button, gpointer user_data) {
    int *pair_index = (int *)user_data;
    
    
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));
    GTKAppData *app = g_object_get_data(G_OBJECT(toplevel), "app_data");
    
    if (app && app->callbacks.on_remove_pair) {
        app->callbacks.on_remove_pair(*pair_index, app->user_data);
    }
    
    g_free(pair_index);
}

static void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    if (app->callbacks.on_refresh) {
        app->callbacks.on_refresh(app->user_data);
    }
}

static void on_theme_toggle_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    app->dark_theme = !app->dark_theme;
    apply_theme(app);
    
    if (app->callbacks.on_theme_toggle) {
        app->callbacks.on_theme_toggle(app->user_data);
    }
}

static void on_rebalance_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    if (app->callbacks.on_show_optimization) {
        app->callbacks.on_show_optimization(app->user_data);
    }
    
    show_optimization_dialog(app);
}

static void on_opportunities_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    if (app->callbacks.on_show_opportunities) {
        app->callbacks.on_show_opportunities(app->user_data);
    }
    
    show_opportunities_dialog(app);
}

static void on_bots_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    show_bots_dialog(app);
}

static void on_toggle_border_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    gboolean decorated = gtk_window_get_decorated(GTK_WINDOW(app->window));
    gtk_window_set_decorated(GTK_WINDOW(app->window), !decorated);
}

static gboolean on_card_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    int *pair_index = (int *)user_data;
    
    if (event->type == GDK_DOUBLE_BUTTON_PRESS && event->button == 1) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
        GTKAppData *app = g_object_get_data(G_OBJECT(toplevel), "app_data");
        
        if (app) {
            show_edit_pair_dialog(app, *pair_index);
        }
        return TRUE;
    }
    
    return FALSE;
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    
    if (app->portfolio) {
        portfolio_save(app->portfolio);
    }
    
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
    }
    if (app->animation_timer > 0) {
        g_source_remove(app->animation_timer);
    }
    
    gtk_main_quit();
}

static gboolean timer_callback(gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    if (app->callbacks.on_refresh) {
        app->callbacks.on_refresh(app->user_data);
    }
    
    return TRUE;
}


static void show_edit_pair_dialog(GTKAppData *app, int pair_index) {
    if (pair_index < 0 || pair_index >= app->portfolio->pair_count) {
        return;
    }
    
    TradingPair *pair = &app->portfolio->pairs[pair_index];
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Edit Trading Pair / Position",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_OK,
        NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 12);
    gtk_widget_set_margin_bottom(grid, 12);
    
    GtkWidget *symbol_label = gtk_label_new("Symbol:");
    GtkWidget *symbol_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(symbol_entry), pair->symbol);
    
    GtkWidget *price_label = gtk_label_new("Entry Price:");
    GtkWidget *price_entry = gtk_entry_new();
    char price_str[64];
    snprintf(price_str, sizeof(price_str), "%.2f", pair->bought_price);
    gtk_entry_set_text(GTK_ENTRY(price_entry), price_str);
    
    GtkWidget *quantity_label = gtk_label_new("Quantity:");
    GtkWidget *quantity_entry = gtk_entry_new();
    char qty_str[64];
    snprintf(qty_str, sizeof(qty_str), "%.4f", pair->quantity);
    gtk_entry_set_text(GTK_ENTRY(quantity_entry), qty_str);
    
    
    GtkWidget *type_label = gtk_label_new("Position Type:");
    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *long_radio = gtk_radio_button_new_with_label(NULL, "LONG (Buy)");
    GtkWidget *short_radio = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(long_radio), "SHORT (Sell)");
    gtk_box_pack_start(GTK_BOX(type_box), long_radio, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(type_box), short_radio, FALSE, FALSE, 0);
    
    
    if (pair->position_type == POSITION_LONG) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(long_radio), TRUE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(short_radio), TRUE);
    }
    
    gtk_grid_attach(GTK_GRID(grid), symbol_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), symbol_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), price_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), price_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quantity_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quantity_entry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), type_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), type_box, 1, 3, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        const char *symbol = gtk_entry_get_text(GTK_ENTRY(symbol_entry));
        const char *price_str_new = gtk_entry_get_text(GTK_ENTRY(price_entry));
        const char *qty_str_new = gtk_entry_get_text(GTK_ENTRY(quantity_entry));
        gboolean is_long = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(long_radio));
        
        if (strlen(symbol) > 0 && strlen(price_str_new) > 0 && strlen(qty_str_new) > 0) {
            double price = atof(price_str_new);
            double quantity = atof(qty_str_new);
            PositionType pos_type = is_long ? POSITION_LONG : POSITION_SHORT;
            
            if (app->callbacks.on_edit_pair) {
                app->callbacks.on_edit_pair(pair_index, symbol, price, quantity, pos_type, app->user_data);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

static void show_optimization_dialog(GTKAppData *app) {
    double total_value = portfolio_get_total_value(app->portfolio);
    double total_cost = portfolio_get_total_cost(app->portfolio);
    double total_profit = portfolio_get_total_profit_loss(app->portfolio);
    double roi_percent = portfolio_get_total_profit_loss_percent(app->portfolio);
    
    if (total_value <= 0 || app->portfolio->pair_count == 0) {
        GtkWidget *error_dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Portfolio is empty or prices not loaded yet");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        return;
    }
    
    PerformanceItem items[MAX_PAIRS];
    int item_count = 0;
    portfolio_analyze_performance(app->portfolio, items, &item_count);
    
    
    for (int i = 0; i < item_count - 1; i++) {
        for (int j = i + 1; j < item_count; j++) {
            if (items[j].profit_loss_percent > items[i].profit_loss_percent) {
                PerformanceItem temp = items[i];
                items[i] = items[j];
                items[j] = temp;
            }
        }
    }
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Portfolio Growth Opportunities",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 550, 500);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 16);
    
    
    GtkWidget *header_label = gtk_label_new(NULL);
    char header_text[512];
    snprintf(header_text, sizeof(header_text), 
             "<b><span size='large'>Portfolio Analysis &amp; Growth Opportunities</span></b>\n"
             "Total Value: <b>$%.2f</b>  |  Invested: $%.2f  |  P/L: <span foreground='%s'><b>%+.2f%% ($%+.2f)</b></span>", 
             total_value, total_cost,
             total_profit >= 0 ? "#30d158" : "#ff453a",
             roi_percent, total_profit);
    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), header_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
    
    
    GtkWidget *strategy_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(strategy_box, 8);
    gtk_widget_set_margin_end(strategy_box, 8);
    
    GtkWidget *strategy_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(strategy_title), "<b>[R] Recommended Actions</b>");
    gtk_widget_set_halign(strategy_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(strategy_box), strategy_title, FALSE, FALSE, 0);
    
    
    int strong_performers = 0, weak_performers = 0;
    PerformanceItem *best = NULL, *worst = NULL;
    
    for (int i = 0; i < item_count; i++) {
        if (items[i].profit_loss_percent > 10) strong_performers++;
        if (items[i].profit_loss_percent < -10) weak_performers++;
        if (!best || items[i].profit_loss_percent > best->profit_loss_percent) best = &items[i];
        if (!worst || items[i].profit_loss_percent < worst->profit_loss_percent) worst = &items[i];
    }
    
    char rec_text[1024] = "<span size='small'>";
    char *p = rec_text + strlen(rec_text);
    
    if (best) {
        char upper_sym[MAX_SYMBOL_LEN];
        strncpy(upper_sym, best->symbol, MAX_SYMBOL_LEN);
        for (int i = 0; upper_sym[i]; i++) upper_sym[i] = toupper((unsigned char)upper_sym[i]);
        
        p += sprintf(p, "[+] <span foreground='#30d158'><b>Top Winner:</b></span> %s is up <b>%.1f%%</b>. ", upper_sym, best->profit_loss_percent);
        if (best->trend == 1) {
            p += sprintf(p, "Still trending up - consider adding more.\n");
        } else if (best->trend == -1) {
            p += sprintf(p, "Showing downward trend - consider taking profits.\n");
        } else {
            p += sprintf(p, "Monitor for continuation.\n");
        }
    }
    
    if (worst) {
        char upper_sym[MAX_SYMBOL_LEN];
        strncpy(upper_sym, worst->symbol, MAX_SYMBOL_LEN);
        for (int i = 0; upper_sym[i]; i++) upper_sym[i] = toupper((unsigned char)upper_sym[i]);
        
        p += sprintf(p, "\n[!] <span foreground='#ff453a'><b>Underperformer:</b></span> %s is down <b>%.1f%%</b>. ", 
                    upper_sym, fabs(worst->profit_loss_percent));
        if (worst->trend == 1) {
            p += sprintf(p, "Showing recovery - could be buying opportunity.\n");
        } else if (worst->trend == -1) {
            p += sprintf(p, "Still declining - consider cutting losses.\n");
        } else {
            p += sprintf(p, "Stabilizing - wait for clearer signals.\n");
        }
    }
    sprintf(p, "</span>");
    
    GtkWidget *rec_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(rec_label), rec_text);
    gtk_label_set_line_wrap(GTK_LABEL(rec_label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(rec_label), 65);
    gtk_widget_set_halign(rec_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(strategy_box), rec_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), strategy_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
    
    
    GtkWidget *details_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(details_title), "<b>Asset Performance</b>");
    gtk_widget_set_halign(details_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start(details_title, 8);
    gtk_box_pack_start(GTK_BOX(main_box), details_title, FALSE, FALSE, 0);
    
    for (int i = 0; i < item_count; i++) {
        PerformanceItem *item = &items[i];
        TradingPair *pair = NULL;
        
        
        for (int j = 0; j < app->portfolio->pair_count; j++) {
            if (strcmp(app->portfolio->pairs[j].symbol, item->symbol) == 0) {
                pair = &app->portfolio->pairs[j];
                break;
            }
        }
        
        GtkWidget *item_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_margin_start(item_box, 12);
        gtk_widget_set_margin_end(item_box, 12);
        gtk_widget_set_margin_top(item_box, 6);
        gtk_widget_set_margin_bottom(item_box, 6);
        
        
        char upper_sym[MAX_SYMBOL_LEN];
        strncpy(upper_sym, item->symbol, MAX_SYMBOL_LEN);
        for (int j = 0; upper_sym[j]; j++) upper_sym[j] = toupper((unsigned char)upper_sym[j]);
        
        const char *trend_icon = item->trend == 1 ? "↗" : (item->trend == -1 ? "↘" : "→");
        const char *trend_color = item->trend == 1 ? "#30d158" : (item->trend == -1 ? "#ff453a" : "#98989d");
        
        char symbol_text[256];
        snprintf(symbol_text, sizeof(symbol_text), 
                "<b>%s</b>  <span foreground='%s'>%s</span>  P/L: <span foreground='%s'><b>%+.2f%%</b> ($%+.2f)</span>",
                upper_sym, trend_color, trend_icon,
                item->profit_loss_value >= 0 ? "#30d158" : "#ff453a",
                item->profit_loss_percent, item->profit_loss_value);
        
        GtkWidget *symbol_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(symbol_label), symbol_text);
        gtk_widget_set_halign(symbol_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(item_box), symbol_label, FALSE, FALSE, 0);
        
        
        if (pair && (pair->historical_loaded || pair->historical_1h_loaded)) {
            double rsi = portfolio_calculate_rsi(pair);
            double volatility = portfolio_calculate_volatility(pair);
            const char *rsi_color = rsi > 70 ? "#ff453a" : (rsi < 30 ? "#30d158" : "#98989d");
            
            char details_text[512];
            snprintf(details_text, sizeof(details_text),
                    "<span size='small'>Value: $%.2f  |  Bought: $%.4f → Now: $%.4f  |  Momentum: %.2f%%\n"
                    "RSI: <span foreground='%s'>%.1f</span>  |  Volatility: %.2f%%</span>",
                    item->current_value, item->bought_price, item->current_price, item->momentum_score,
                    rsi_color, rsi, volatility * 100);
            
            GtkWidget *details_label = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(details_label), details_text);
            gtk_widget_set_halign(details_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(item_box), details_label, FALSE, FALSE, 0);
            
            
            if (pair->historical_1h_loaded && pair->historical_1h_count > 20) {
                char enhanced_ta_text[512];
                const char *prob_color;
                const char *prob_confidence;
                double prob_pct = pair->profit_probability * 100.0;
                
                if (prob_pct >= 70) {
                    prob_color = "#30d158";
                    prob_confidence = "HIGH";
                } else if (prob_pct >= 55) {
                    prob_color = "#ffd60a";
                    prob_confidence = "MEDIUM";
                } else if (prob_pct >= 45) {
                    prob_color = "#8e8e93";
                    prob_confidence = "NEUTRAL";
                } else if (prob_pct >= 30) {
                    prob_color = "#ff9500";
                    prob_confidence = "LOW";
                } else {
                    prob_color = "#ff453a";
                    prob_confidence = "VERY LOW";
                }
                
                int mtf_trend = calculate_trend_multi_timeframe(pair);
                const char *trend_status = mtf_trend == 1 ? "All Bullish!" : 
                                          mtf_trend == -1 ? "All Bearish" : 
                                          "Mixed";
                const char *trend_color = mtf_trend == 1 ? "#30d158" : 
                                         mtf_trend == -1 ? "#ff453a" : 
                                         "#8e8e93";
                
                snprintf(enhanced_ta_text, sizeof(enhanced_ta_text),
                        "<span size='small'>[P] Profit Probability: <span foreground='%s'><b>%.1f%%</b></span> (%s)\n"
                        "[*] Patterns: <b>%s</b>  |  [T] Trend: <span foreground='%s'><b>%s</b></span></span>",
                        prob_color, prob_pct, prob_confidence,
                        pair->pattern_count > 0 ? pair->detected_patterns : "None",
                        trend_color, trend_status);
                
                GtkWidget *enhanced_ta_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(enhanced_ta_label), enhanced_ta_text);
                gtk_widget_set_halign(enhanced_ta_label, GTK_ALIGN_START);
                gtk_label_set_line_wrap(GTK_LABEL(enhanced_ta_label), TRUE);
                gtk_label_set_max_width_chars(GTK_LABEL(enhanced_ta_label), 60);
                gtk_box_pack_start(GTK_BOX(item_box), enhanced_ta_label, FALSE, FALSE, 0);
            }
            
            
            if (pair->historical_count > 20 || pair->historical_1h_count > 20) {
                GtkWidget *strategy_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                gtk_widget_set_margin_top(strategy_box, 4);
                
                char strategy_text[600];
                if (item->profit_loss_percent < -10) {
                    
                    double prob_pct = pair->profit_probability * 100.0;
                    if (prob_pct > 60 && item->trend >= 0) {
                        snprintf(strategy_text, sizeof(strategy_text),
                                "<span size='small' foreground='#30d158'><b>[i] Recovery Strategy:</b> High probability of reversal detected. "
                                "Consider averaging down if fundamentals are strong. Wait for confirmation before adding.</span>");
                    } else if (item->trend == -1) {
                        snprintf(strategy_text, sizeof(strategy_text),
                                "<span size='small' foreground='#ff453a'><b>[!] Cut Loss Strategy:</b> Downtrend continuing. "
                                "Consider cutting losses if position breaks below support. Set stop-loss at -15%%.</span>");
                    } else {
                        snprintf(strategy_text, sizeof(strategy_text),
                                "<span size='small' foreground='#ff9500'><b>[~] Wait Strategy:</b> Consolidating. "
                                "Hold current position and wait for clearer signals before making changes.</span>");
                    }
                    
                    GtkWidget *strategy_label = gtk_label_new(NULL);
                    gtk_label_set_markup(GTK_LABEL(strategy_label), strategy_text);
                    gtk_widget_set_halign(strategy_label, GTK_ALIGN_START);
                    gtk_label_set_line_wrap(GTK_LABEL(strategy_label), TRUE);
                    gtk_label_set_max_width_chars(GTK_LABEL(strategy_label), 60);
                    gtk_box_pack_start(GTK_BOX(strategy_box), strategy_label, FALSE, FALSE, 0);
                } else if (item->profit_loss_percent > 15) {
                    
                    if (item->trend == -1) {
                        snprintf(strategy_text, sizeof(strategy_text),
                                "<span size='small' foreground='#ff9500'><b>[v] Take Profit:</b> Uptrend weakening. "
                                "Consider taking 50%% profits now and letting rest run with trailing stop.</span>");
                    } else if (item->trend == 1) {
                        snprintf(strategy_text, sizeof(strategy_text),
                                "<span size='small' foreground='#30d158'><b>[^] Partial Profit:</b> Still trending up. "
                                "Take 25%% profits as insurance, hold rest for higher targets.</span>");
                    } else {
                        snprintf(strategy_text, sizeof(strategy_text),
                                "<span size='small' foreground='#ffd60a'><b>[$] Secure Gains:</b> Good profit achieved. "
                                "Consider taking 30-40%% profits and raising stop-loss to break-even.</span>");
                    }
                    
                    GtkWidget *strategy_label = gtk_label_new(NULL);
                    gtk_label_set_markup(GTK_LABEL(strategy_label), strategy_text);
                    gtk_widget_set_halign(strategy_label, GTK_ALIGN_START);
                    gtk_label_set_line_wrap(GTK_LABEL(strategy_label), TRUE);
                    gtk_label_set_max_width_chars(GTK_LABEL(strategy_label), 60);
                    gtk_box_pack_start(GTK_BOX(strategy_box), strategy_label, FALSE, FALSE, 0);
                }
                
                if (strlen(strategy_text) > 0) {
                    gtk_box_pack_start(GTK_BOX(item_box), strategy_box, FALSE, FALSE, 0);
                }
            }
            
            
            if (pair->historical_count > 20 || pair->historical_1h_count > 20) {
                double buy_price = 0, sell_price = 0;
                char buy_reason[128] = "", sell_reason[128] = "";
                portfolio_calculate_trade_prices(pair, &buy_price, &sell_price, buy_reason, sell_reason);
                
                GtkWidget *trade_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                gtk_widget_set_margin_top(trade_box, 4);
                
                char buy_markup[300];
                const char *buy_color = buy_price < item->current_price ? "#30d158" : "#ff9500";
                double buy_diff = ((buy_price - item->current_price) / item->current_price) * 100.0;
                snprintf(buy_markup, sizeof(buy_markup),
                        "<span size='small'>[B] <b>Buy At:</b> <span foreground='%s'>$%.6f</span> (%+.1f%%) - %s</span>",
                        buy_color, buy_price, buy_diff, buy_reason);
                
                GtkWidget *buy_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(buy_label), buy_markup);
                gtk_widget_set_halign(buy_label, GTK_ALIGN_START);
                gtk_box_pack_start(GTK_BOX(trade_box), buy_label, FALSE, FALSE, 0);
                
                char sell_markup[300];
                const char *sell_color = sell_price > item->current_price ? "#30d158" : "#ff453a";
                double sell_diff = ((sell_price - item->current_price) / item->current_price) * 100.0;
                snprintf(sell_markup, sizeof(sell_markup),
                        "<span size='small'>[S] <b>Sell At:</b> <span foreground='%s'>$%.6f</span> (%+.1f%%) - %s</span>",
                        sell_color, sell_price, sell_diff, sell_reason);
                
                GtkWidget *sell_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(sell_label), sell_markup);
                gtk_widget_set_halign(sell_label, GTK_ALIGN_START);
                gtk_box_pack_start(GTK_BOX(trade_box), sell_label, FALSE, FALSE, 0);
                
                gtk_box_pack_start(GTK_BOX(item_box), trade_box, FALSE, FALSE, 0);
            }
        }
        
        gtk_box_pack_start(GTK_BOX(main_box), item_box, FALSE, FALSE, 0);
        
        if (i < item_count - 1) {
            GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_widget_set_margin_start(separator, 12);
            gtk_widget_set_margin_end(separator, 12);
            gtk_box_pack_start(GTK_BOX(main_box), separator, FALSE, FALSE, 0);
        }
    }
    
    gtk_container_add(GTK_CONTAINER(scrolled), main_box);
    gtk_box_pack_start(GTK_BOX(content_area), scrolled, TRUE, TRUE, 0);
    
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}


typedef struct {
    GTKAppData *app;
    InvestmentOpportunity *opportunities;
    int *opp_count;
    GtkWidget *dialog;
    NetworkManager *network;
    int pending_requests;
} OpportunityFetchData;

static const char* POPULAR_PAIRS[] = {
    "BTCUSDT", "ETHUSDT", "BNBUSDT", "SOLUSDT", "XRPUSDT",
    "ADAUSDT", "DOGEUSDT", "MATICUSDT", "DOTUSDT", "AVAXUSDT",
    "LTCUSDT", "LINKUSDT", "ATOMUSDT", "UNIUSDT", "ARBUSDT"
};
static const int POPULAR_PAIRS_COUNT = 15;

static int compare_opportunities(const void *a, const void *b) {
    const InvestmentOpportunity *opp_a = (const InvestmentOpportunity *)a;
    const InvestmentOpportunity *opp_b = (const InvestmentOpportunity *)b;
    
    if (opp_a->score > opp_b->score) return -1;
    if (opp_a->score < opp_b->score) return 1;
    return 0;
}

static void update_opportunities_display(OpportunityFetchData *fetch_data, GtkWidget *main_box, 
                                         GtkWidget *header_label, GtkWidget *separator1);

static void fetch_opportunity_24h_callback(SoupSession *session, SoupMessage *msg, gpointer user_data) {
    OpportunityFetchData *data = (OpportunityFetchData *)user_data;
    
    
    if (!data->dialog || !GTK_IS_WIDGET(data->dialog)) {
        
        data->pending_requests--;
        if (data->pending_requests == 0) {
            g_free(data->opportunities);
            g_free(data->opp_count);
            g_free(data);
        }
        return;
    }
    
    if (msg->status_code == 200) {
        struct json_object *root = json_tokener_parse(msg->response_body->data);
        if (root) {
            struct json_object *symbol_obj, *price_obj, *change_obj, *volume_obj;
            
            if (json_object_object_get_ex(root, "symbol", &symbol_obj) &&
                json_object_object_get_ex(root, "lastPrice", &price_obj) &&
                json_object_object_get_ex(root, "priceChangePercent", &change_obj) &&
                json_object_object_get_ex(root, "volume", &volume_obj)) {
                
                const char *symbol = json_object_get_string(symbol_obj);
                
                
                gboolean already_owned = FALSE;
                for (int i = 0; i < data->app->portfolio->pair_count; i++) {
                    char upper_symbol[MAX_SYMBOL_LEN];
                    strncpy(upper_symbol, data->app->portfolio->pairs[i].symbol, MAX_SYMBOL_LEN - 1);
                    for (int j = 0; upper_symbol[j]; j++) {
                        upper_symbol[j] = toupper((unsigned char)upper_symbol[j]);
                    }
                    if (strcmp(symbol, upper_symbol) == 0) {
                        already_owned = TRUE;
                        break;
                    }
                }
                
                if (!already_owned && *data->opp_count < 15) {
                    InvestmentOpportunity *opp = &data->opportunities[*data->opp_count];
                    
                    
                    strncpy(opp->symbol, symbol, MAX_SYMBOL_LEN - 1);
                    for (int j = 0; opp->symbol[j]; j++) {
                        opp->symbol[j] = tolower((unsigned char)opp->symbol[j]);
                    }
                    
                    opp->current_price = json_object_get_double(price_obj);
                    opp->price_change_24h = json_object_get_double(change_obj);
                    opp->volume_24h = json_object_get_double(volume_obj);
                    
                    
                    opp->score = 0;
                    if (opp->price_change_24h > 5.0) opp->score += 3;
                    else if (opp->price_change_24h > 2.0) opp->score += 2;
                    else if (opp->price_change_24h > 0) opp->score += 1;
                    else if (opp->price_change_24h < -5.0) opp->score += 2; 
                    
                    if (opp->volume_24h > 1000000) opp->score += 2;
                    else if (opp->volume_24h > 100000) opp->score += 1;
                    
                    opp->trend = opp->price_change_24h > 2.0 ? 1 : (opp->price_change_24h < -2.0 ? -1 : 0);
                    opp->momentum = opp->price_change_24h;
                    
                    
                    if (opp->price_change_24h < -5.0) {
                        opp->suggested_buy_price = opp->current_price * 1.01;
                        opp->suggested_sell_price = opp->current_price * 1.15;
                    } else if (opp->price_change_24h > 10.0) {
                        opp->suggested_buy_price = opp->current_price * 0.93;
                        opp->suggested_sell_price = opp->current_price * 1.10;
                    } else if (opp->price_change_24h > 5.0) {
                        opp->suggested_buy_price = opp->current_price * 0.97;
                        opp->suggested_sell_price = opp->current_price * 1.12;
                    } else {
                        opp->suggested_buy_price = opp->current_price * 0.95;
                        opp->suggested_sell_price = opp->current_price * 1.10;
                    }
                    
                    (*data->opp_count)++;
                }
            }
            json_object_put(root);
        }
    }
    
    data->pending_requests--;
    
    
    if (data->pending_requests == 0) {
        printf("Loaded %d investment opportunities\n", *data->opp_count);
        
        
        qsort(data->opportunities, *data->opp_count, sizeof(InvestmentOpportunity), compare_opportunities);
        
        
        if (data->dialog && GTK_IS_WIDGET(data->dialog)) {
            GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(data->dialog));
            GList *children = gtk_container_get_children(GTK_CONTAINER(content_area));
            if (children && children->data) {
                GtkWidget *scrolled = GTK_WIDGET(children->data);
                GList *scrolled_children = gtk_container_get_children(GTK_CONTAINER(scrolled));
                if (scrolled_children && scrolled_children->data) {
                    GtkWidget *child = GTK_WIDGET(scrolled_children->data);
                    GtkWidget *main_box = NULL;
                    if (GTK_IS_VIEWPORT(child)) {
                        main_box = gtk_bin_get_child(GTK_BIN(child));
                    } else {
                        main_box = child;
                    }
                    if (main_box) {
                        GList *box_children = gtk_container_get_children(GTK_CONTAINER(main_box));
                        GtkWidget *header = box_children ? GTK_WIDGET(box_children->data) : NULL;
                        GtkWidget *separator = box_children && box_children->next ? 
                                               GTK_WIDGET(box_children->next->data) : NULL;
                        update_opportunities_display(data, main_box, header, separator);
                        g_list_free(box_children);
                    }
                }
                g_list_free(scrolled_children);
            }
            g_list_free(children);
        }
    }
}

static void discover_investment_opportunities(OpportunityFetchData *data) {
    data->pending_requests = 0;
    
    for (int i = 0; i < POPULAR_PAIRS_COUNT; i++) {
        char url[512];
        snprintf(url, sizeof(url),
                "https://api.binance.com/api/v3/ticker/24hr?symbol=%s",
                POPULAR_PAIRS[i]);
        
        SoupMessage *msg = soup_message_new("GET", url);
        soup_session_queue_message(data->network->session, msg, fetch_opportunity_24h_callback, data);
        data->pending_requests++;
    }
}

static void update_opportunities_display(OpportunityFetchData *fetch_data, GtkWidget *main_box,
                                         GtkWidget *header_label, GtkWidget *separator1) {
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(main_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        if (iter->data != header_label && iter->data != separator1) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
    }
    g_list_free(children);
    
    int opp_count = *fetch_data->opp_count;
    InvestmentOpportunity *opportunities = fetch_data->opportunities;
    
    if (opp_count == 0) {
        GtkWidget *no_opps = gtk_label_new("No new opportunities found.\nYou already own most popular assets!");
        gtk_widget_set_halign(no_opps, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(no_opps, 40);
        gtk_widget_set_margin_bottom(no_opps, 40);
        gtk_box_pack_start(GTK_BOX(main_box), no_opps, TRUE, TRUE, 0);
    } else {
        for (int i = 0; i < opp_count; i++) {
            InvestmentOpportunity *opp = &opportunities[i];
            
            GtkWidget *opp_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
            gtk_widget_set_margin_start(opp_card, 8);
            gtk_widget_set_margin_end(opp_card, 8);
            gtk_widget_set_margin_top(opp_card, 6);
            gtk_widget_set_margin_bottom(opp_card, 6);
            
            
            char symbol_upper[MAX_SYMBOL_LEN];
            strncpy(symbol_upper, opp->symbol, MAX_SYMBOL_LEN - 1);
            for (int j = 0; symbol_upper[j]; j++) {
                symbol_upper[j] = toupper((unsigned char)symbol_upper[j]);
            }
            
            GtkWidget *symbol_label = gtk_label_new(NULL);
            char symbol_text[256];
            const char *trend_icon = opp->trend == 1 ? "↗" : (opp->trend == -1 ? "↘" : "→");
            const char *trend_color = opp->trend == 1 ? "#30d158" : (opp->trend == -1 ? "#ff453a" : "#98989d");
            
            char stars[20] = "";
            for (int s = 0; s < opp->score && s < 5; s++) {
                strcat(stars, "*");
            }
            
            snprintf(symbol_text, sizeof(symbol_text),
                    "<b>%s</b> <span foreground='%s'>%s</span>  %s",
                    symbol_upper, trend_color, trend_icon, stars);
            gtk_label_set_markup(GTK_LABEL(symbol_label), symbol_text);
            gtk_widget_set_halign(symbol_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(opp_card), symbol_label, FALSE, FALSE, 0);
            
            
            GtkWidget *price_label = gtk_label_new(NULL);
            char price_text[256];
            const char *change_color = opp->price_change_24h >= 0 ? "#30d158" : "#ff453a";
            snprintf(price_text, sizeof(price_text),
                    "<span size='small'>Price: $%.6f  |  24h: <span foreground='%s'><b>%+.2f%%</b></span>  |  Volume: $%.0f</span>",
                    opp->current_price, change_color, opp->price_change_24h, opp->volume_24h);
            gtk_label_set_markup(GTK_LABEL(price_label), price_text);
            gtk_widget_set_halign(price_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(opp_card), price_label, FALSE, FALSE, 0);
            
            
            GtkWidget *rec_label = gtk_label_new(NULL);
            char rec_text[512];
            
            if (opp->price_change_24h > 10) {
                snprintf(rec_text, sizeof(rec_text),
                        "<span size='small' foreground='#30d158'>[^] <b>Hot Asset:</b> Strong 24h momentum (+%.1f%%) - trending asset</span>",
                        opp->price_change_24h);
            } else if (opp->price_change_24h > 5) {
                snprintf(rec_text, sizeof(rec_text),
                        "<span size='small' foreground='#30d158'>[+] <b>Rising:</b> Positive momentum (+%.1f%%) - consider for growth</span>",
                        opp->price_change_24h);
            } else if (opp->price_change_24h < -10) {
                snprintf(rec_text, sizeof(rec_text),
                        "<span size='small' foreground='#007AFF'>[o] <b>Dip Opportunity:</b> Down %.1f%% - potential buy at discount</span>",
                        fabs(opp->price_change_24h));
            } else if (opp->price_change_24h < -5) {
                snprintf(rec_text, sizeof(rec_text),
                        "<span size='small' foreground='#007AFF'>[*] <b>Correction:</b> Down %.1f%% - watch for reversal</span>",
                        fabs(opp->price_change_24h));
            } else if (opp->volume_24h > 1000000) {
                snprintf(rec_text, sizeof(rec_text),
                        "<span size='small' foreground='#98989d'>[V] <b>High Volume:</b> Active trading - liquid market</span>");
            } else {
                snprintf(rec_text, sizeof(rec_text),
                        "<span size='small' foreground='#98989d'>[=] <b>Stable:</b> Low volatility - conservative option</span>");
            }
            
            gtk_label_set_markup(GTK_LABEL(rec_label), rec_text);
            gtk_label_set_line_wrap(GTK_LABEL(rec_label), TRUE);
            gtk_widget_set_halign(rec_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(opp_card), rec_label, FALSE, FALSE, 0);
            
            
            GtkWidget *trade_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_widget_set_margin_top(trade_box, 4);
            
            GtkWidget *buy_label = gtk_label_new(NULL);
            char buy_markup[256];
            const char *buy_color = opp->suggested_buy_price < opp->current_price ? "#30d158" : "#ff9500";
            double buy_diff = ((opp->suggested_buy_price - opp->current_price) / opp->current_price) * 100.0;
            snprintf(buy_markup, sizeof(buy_markup),
                    "<span size='small'>[B] <b>Suggested Buy:</b> <span foreground='%s'>$%.6f</span> (%+.1f%%)</span>",
                    buy_color, opp->suggested_buy_price, buy_diff);
            gtk_label_set_markup(GTK_LABEL(buy_label), buy_markup);
            gtk_widget_set_halign(buy_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(trade_box), buy_label, FALSE, FALSE, 0);
            
            GtkWidget *sell_label = gtk_label_new(NULL);
            char sell_markup[256];
            double sell_gain = ((opp->suggested_sell_price - opp->suggested_buy_price) / opp->suggested_buy_price) * 100.0;
            snprintf(sell_markup, sizeof(sell_markup),
                    "<span size='small'>[S] <b>Suggested Sell:</b> <span foreground='#30d158'>$%.6f</span> (+%.1f%% target)</span>",
                    opp->suggested_sell_price, sell_gain);
            gtk_label_set_markup(GTK_LABEL(sell_label), sell_markup);
            gtk_widget_set_halign(sell_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(trade_box), sell_label, FALSE, FALSE, 0);
            
            gtk_box_pack_start(GTK_BOX(opp_card), trade_box, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(main_box), opp_card, FALSE, FALSE, 0);
            
            if (i < opp_count - 1) {
                GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
                gtk_widget_set_margin_start(sep, 12);
                gtk_widget_set_margin_end(sep, 12);
                gtk_box_pack_start(GTK_BOX(main_box), sep, FALSE, FALSE, 0);
            }
        }
    }
    
    gtk_widget_show_all(main_box);
}


static void on_opportunities_dialog_destroy(GtkWidget *widget, gpointer user_data) {
    OpportunityFetchData *fetch_data = (OpportunityFetchData *)user_data;
    
    
    fetch_data->dialog = NULL;
    
    
    if (fetch_data->pending_requests == 0) {
        g_free(fetch_data->opportunities);
        g_free(fetch_data->opp_count);
        g_free(fetch_data);
    }
    
}

static void on_opportunities_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    OpportunityFetchData *fetch_data = (OpportunityFetchData *)user_data;
    
    if (response_id == GTK_RESPONSE_APPLY) {
        
        *fetch_data->opp_count = 0;
        
        
        GtkWidget *content_area = gtk_dialog_get_content_area(dialog);
        GList *children = gtk_container_get_children(GTK_CONTAINER(content_area));
        if (children && children->data) {
            GtkWidget *scrolled = GTK_WIDGET(children->data);
            GList *scrolled_children = gtk_container_get_children(GTK_CONTAINER(scrolled));
            if (scrolled_children && scrolled_children->data) {
                GtkWidget *child = GTK_WIDGET(scrolled_children->data);
                GtkWidget *main_box = NULL;
                if (GTK_IS_VIEWPORT(child)) {
                    main_box = gtk_bin_get_child(GTK_BIN(child));
                } else {
                    main_box = child;
                }
                if (main_box) {
                    GList *box_children = gtk_container_get_children(GTK_CONTAINER(main_box));
                    GtkWidget *header = box_children ? GTK_WIDGET(box_children->data) : NULL;
                    GtkWidget *separator = box_children && box_children->next ? 
                                           GTK_WIDGET(box_children->next->data) : NULL;
                    
                    
                    for (GList *iter = box_children; iter != NULL; iter = g_list_next(iter)) {
                        if (iter->data != header && iter->data != separator) {
                            gtk_widget_destroy(GTK_WIDGET(iter->data));
                        }
                    }
                    
                    
                    GtkWidget *loading_label = gtk_label_new("Refreshing market data...");
                    gtk_widget_set_halign(loading_label, GTK_ALIGN_CENTER);
                    gtk_widget_set_margin_top(loading_label, 40);
                    gtk_widget_set_margin_bottom(loading_label, 40);
                    gtk_box_pack_start(GTK_BOX(main_box), loading_label, TRUE, TRUE, 0);
                    gtk_widget_show_all(main_box);
                    
                    g_list_free(box_children);
                }
                g_list_free(scrolled_children);
            }
            g_list_free(children);
        }
        
        
        discover_investment_opportunities(fetch_data);
    } else {
        
        
        gtk_widget_destroy(GTK_WIDGET(dialog));
    }
}

static void show_opportunities_dialog(GTKAppData *app) {
    
    InvestmentOpportunity *opportunities = g_malloc0(15 * sizeof(InvestmentOpportunity));
    int *opp_count = g_malloc0(sizeof(int));
    *opp_count = 0;
    
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Investment Opportunities",
        GTK_WINDOW(app->window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        "Refresh", GTK_RESPONSE_APPLY,
        "Close", GTK_RESPONSE_CLOSE,
        NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 500);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 16);
    
    
    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header_label),
                        "<b><span size='large'>[O] Discover New Investment Opportunities</span></b>\n"
                        "<span size='small'>Popular crypto assets not currently in your portfolio</span>");
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), header_label, FALSE, FALSE, 0);
    
    
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_box), separator1, FALSE, FALSE, 4);
    
    
    GtkWidget *loading_label = gtk_label_new("Loading market data...");
    gtk_widget_set_halign(loading_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(loading_label, 40);
    gtk_widget_set_margin_bottom(loading_label, 40);
    gtk_box_pack_start(GTK_BOX(main_box), loading_label, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(scrolled), main_box);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled);
    gtk_widget_show_all(content_area);
    
    
    OpportunityFetchData *fetch_data = g_malloc(sizeof(OpportunityFetchData));
    fetch_data->app = app;
    fetch_data->opportunities = opportunities;
    fetch_data->opp_count = opp_count;
    fetch_data->dialog = dialog;
    fetch_data->network = app->network;
    fetch_data->pending_requests = 0;
    
    
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_opportunities_dialog_destroy), fetch_data);
    
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_opportunities_dialog_response), fetch_data);
    
    
    gtk_widget_show_all(dialog);
    
    
    discover_investment_opportunities(fetch_data);
}


static void apply_css_styling(GTKAppData *app) {
    GtkCssProvider *provider = gtk_css_provider_new();
    
    const char *css = 
        "\n"
        ".light-theme window { background: #f5f5f7; }\n"
        ".light-theme #title-label { color: #1d1d1f; font-size: 24px; font-weight: 700; }\n"
        ".light-theme #total-label { color: #1d1d1f; font-size: 18px; font-weight: 600; }\n"
        ".light-theme #portfolio-card { background: white; border-radius: 12px; padding: 12px; margin: 4px; }\n"
        ".light-theme #symbol-label { color: #1d1d1f; font-size: 16px; font-weight: 600; }\n"
        ".light-theme #price-label { color: #1d1d1f; font-size: 20px; font-weight: 700; }\n"
        ".light-theme #holdings-label { color: #86868b; font-size: 12px; }\n"
        ".light-theme #profit-label { color: #30d158; font-weight: 600; }\n"
        ".light-theme #loss-label { color: #ff453a; font-weight: 600; }\n"
        "\n\n"
        ".dark-theme window { background: #1c1c1e; }\n"
        ".dark-theme #title-label { color: #f5f5f7; font-size: 24px; font-weight: 700; }\n"
        ".dark-theme #total-label { color: #f5f5f7; font-size: 18px; font-weight: 600; }\n"
        ".dark-theme #portfolio-card { background: #2c2c2e; border-radius: 12px; padding: 12px; margin: 4px; }\n"
        ".dark-theme #symbol-label { color: #f5f5f7; font-size: 16px; font-weight: 600; }\n"
        ".dark-theme #price-label { color: #f5f5f7; font-size: 20px; font-weight: 700; }\n"
        ".dark-theme #holdings-label { color: #98989d; font-size: 12px; }\n"
        ".dark-theme #profit-label { color: #30d158; font-weight: 600; }\n"
        ".dark-theme #loss-label { color: #ff453a; font-weight: 600; }\n"
        "\nbutton { border-radius: 8px; padding: 8px 16px; font-weight: 600; }\n"
        "#remove-button { background: rgba(255, 69, 58, 0.1); color: #ff453a; border-radius: 6px; padding: 2px 6px; }\n"
        "#remove-button:hover { background: rgba(255, 69, 58, 0.3); }\n";
    
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(provider);
}

static void apply_theme(GTKAppData *app) {
    GtkStyleContext *context = gtk_widget_get_style_context(app->window);
    
    if (app->dark_theme) {
        gtk_style_context_remove_class(context, "light-theme");
        gtk_style_context_add_class(context, "dark-theme");
        gtk_button_set_label(GTK_BUTTON(app->theme_button), "☼");
        gtk_widget_set_tooltip_text(app->theme_button, "Switch to Light Theme");
    } else {
        gtk_style_context_remove_class(context, "dark-theme");
        gtk_style_context_add_class(context, "light-theme");
        gtk_button_set_label(GTK_BUTTON(app->theme_button), "☾");
        gtk_widget_set_tooltip_text(app->theme_button, "Switch to Dark Theme");
    }
}




typedef struct {
    GTKAppData *app;
    int bot_index;
} BotButtonData;


typedef struct {
    OpportunityFetchData *fetch_data;
    gboolean *is_active;
} OpportunityCallbackData;

static void on_bot_start_clicked(GtkButton *button, gpointer user_data) {
    BotButtonData *data = (BotButtonData *)user_data;
    bot_start(data->app->bot_manager, data->bot_index);
    
    
    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(button));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(content));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    
    gtk_widget_destroy(dialog);
    show_bots_dialog(data->app);
    g_free(data);
}

static void on_bot_pause_clicked(GtkButton *button, gpointer user_data) {
    BotButtonData *data = (BotButtonData *)user_data;
    bot_pause(data->app->bot_manager, data->bot_index);
    
    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(button));
    gtk_widget_destroy(dialog);
    show_bots_dialog(data->app);
    g_free(data);
}

static void on_bot_stop_clicked(GtkButton *button, gpointer user_data) {
    BotButtonData *data = (BotButtonData *)user_data;
    bot_stop(data->app->bot_manager, data->bot_index);
    
    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(button));
    gtk_widget_destroy(dialog);
    show_bots_dialog(data->app);
    g_free(data);
}

static void on_bot_reset_clicked(GtkButton *button, gpointer user_data) {
    BotButtonData *data = (BotButtonData *)user_data;
    bot_reset(data->app->bot_manager, data->bot_index);
    
    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(button));
    gtk_widget_destroy(dialog);
    show_bots_dialog(data->app);
    g_free(data);
}

static void on_bot_remove_clicked(GtkButton *button, gpointer user_data) {
    BotButtonData *data = (BotButtonData *)user_data;
    bot_remove(data->app->bot_manager, data->bot_index);
    
    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(button));
    gtk_widget_destroy(dialog);
    show_bots_dialog(data->app);
    g_free(data);
}


static void on_add_bot_clicked(GtkButton *button, gpointer user_data) {
    GTKAppData *app = (GTKAppData *)user_data;
    
    if (!app->bot_manager) {
        GtkWidget *error_dialog = gtk_message_dialog_new(
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Bot manager not available");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        return;
    }
    
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Add Scalping Bot",
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Add Bot", GTK_RESPONSE_OK,
        NULL);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 16);
    
    
    GtkWidget *symbol_label = gtk_label_new("Trading Pair:");
    gtk_widget_set_halign(symbol_label, GTK_ALIGN_START);
    GtkWidget *symbol_combo = gtk_combo_box_text_new();
    
    
    for (int i = 0; i < app->portfolio->pair_count; i++) {
        char upper_symbol[MAX_SYMBOL_LEN];
        strncpy(upper_symbol, app->portfolio->pairs[i].symbol, MAX_SYMBOL_LEN);
        for (int j = 0; upper_symbol[j]; j++) {
            upper_symbol[j] = toupper((unsigned char)upper_symbol[j]);
        }
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(symbol_combo), upper_symbol);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(symbol_combo), 0);
    
    
    GtkWidget *balance_label = gtk_label_new("Initial Balance ($):");
    gtk_widget_set_halign(balance_label, GTK_ALIGN_START);
    GtkWidget *balance_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(balance_entry), "1000");
    gtk_entry_set_placeholder_text(GTK_ENTRY(balance_entry), "e.g., 1000");
    
    
    GtkWidget *amount_label = gtk_label_new("Trade Amount ($):");
    gtk_widget_set_halign(amount_label, GTK_ALIGN_START);
    GtkWidget *amount_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(amount_entry), "100");
    gtk_entry_set_placeholder_text(GTK_ENTRY(amount_entry), "e.g., 100");
    
    
    GtkWidget *info_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(info_label),
        "<span size='small'><i>Bot will execute mock trades based on scalping signals.\n"
        "Fees (0.1%) are automatically deducted from each trade.</i></span>");
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    
    
    gtk_grid_attach(GTK_GRID(grid), symbol_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), symbol_combo, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), balance_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), balance_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), amount_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), amount_entry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), info_label, 0, 3, 2, 1);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (result == GTK_RESPONSE_OK) {
        const char *symbol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(symbol_combo));
        const char *balance_text = gtk_entry_get_text(GTK_ENTRY(balance_entry));
        const char *amount_text = gtk_entry_get_text(GTK_ENTRY(amount_entry));
        
        double balance = atof(balance_text);
        double amount = atof(amount_text);
        
        if (balance > 0 && amount > 0 && amount <= balance) {
            int bot_index = bot_add(app->bot_manager, symbol, balance, amount);
            if (bot_index >= 0) {
                bot_start(app->bot_manager, bot_index);
                
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "[OK] Bot #%d created and RUNNING!\n\n"
                    "Symbol: %s\n"
                    "Balance: $%.2f\n"
                    "Trade Amount: $%.2f\n\n"
                    "Bot is now actively monitoring signals.",
                    bot_index + 1, symbol, balance, amount);
                
                GtkWidget *success = gtk_message_dialog_new(
                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_INFO,
                    GTK_BUTTONS_OK,
                    "%s", msg);
                gtk_dialog_run(GTK_DIALOG(success));
                gtk_widget_destroy(success);
                
                
                bot_update_statistics(&app->bot_manager->bots[bot_index]);
            } else {
                GtkWidget *error = gtk_message_dialog_new(
                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Failed to create bot. Maximum %d bots allowed.", MAX_BOTS);
                gtk_dialog_run(GTK_DIALOG(error));
                gtk_widget_destroy(error);
            }
        } else {
            GtkWidget *error = gtk_message_dialog_new(
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Invalid values. Ensure balance > 0 and trade amount <= balance.");
            gtk_dialog_run(GTK_DIALOG(error));
            gtk_widget_destroy(error);
        }
    }
    
    gtk_widget_destroy(dialog);
}

static void show_bots_dialog(GTKAppData *app) {
    if (!app->bot_manager) {
        GtkWidget *error = gtk_message_dialog_new(
            GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Bot manager not available");
        gtk_dialog_run(GTK_DIALOG(error));
        gtk_widget_destroy(error);
        return;
    }
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Scalping Bots Manager",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 500);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 16);
    
    
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header),
        "<b><span size='large'>[BOT] Scalping Bots</span></b>\n"
        "<span size='small'>Automated mock trading based on real-time signals</span>");
    gtk_widget_set_halign(header, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);
    
    
    GtkWidget *add_button = gtk_button_new_with_label("[+] Add New Bot");
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_bot_clicked), app);
    gtk_box_pack_start(GTK_BOX(main_box), add_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
    
    
    int active_bots = 0;
    for (int i = 0; i < MAX_BOTS; i++) {
        if (!app->bot_manager->bots[i].active) continue;
        
        ScalpingBot *bot = &app->bot_manager->bots[i];
        active_bots++;
        
        
        double current_price = 0.0;
        for (int j = 0; j < app->portfolio->pair_count; j++) {
            char upper_pair[MAX_SYMBOL_LEN];
            strncpy(upper_pair, app->portfolio->pairs[j].symbol, MAX_SYMBOL_LEN);
            for (int k = 0; upper_pair[k]; k++) {
                upper_pair[k] = toupper((unsigned char)upper_pair[k]);
            }
            
            char upper_bot[MAX_SYMBOL_LEN];
            strncpy(upper_bot, bot->symbol, MAX_SYMBOL_LEN);
            for (int k = 0; upper_bot[k]; k++) {
                upper_bot[k] = toupper((unsigned char)upper_bot[k]);
            }
            
            if (strcmp(upper_pair, upper_bot) == 0) {
                current_price = app->portfolio->pairs[j].current_price;
                break;
            }
        }
        
        
        GtkWidget *bot_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_start(bot_card, 12);
        gtk_widget_set_margin_end(bot_card, 12);
        gtk_widget_set_margin_top(bot_card, 8);
        gtk_widget_set_margin_bottom(bot_card, 8);
        
        
        GtkWidget *bot_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        
        char upper_symbol[MAX_SYMBOL_LEN];
        strncpy(upper_symbol, bot->symbol, MAX_SYMBOL_LEN);
        for (int j = 0; upper_symbol[j]; j++) {
            upper_symbol[j] = toupper((unsigned char)upper_symbol[j]);
        }
        
        GtkWidget *symbol_label = gtk_label_new(NULL);
        char symbol_markup[128];
        snprintf(symbol_markup, sizeof(symbol_markup),
                "<b><span size='large'>Bot #%d - %s</span></b>", i + 1, upper_symbol);
        gtk_label_set_markup(GTK_LABEL(symbol_label), symbol_markup);
        gtk_widget_set_halign(symbol_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand(symbol_label, TRUE);
        gtk_box_pack_start(GTK_BOX(bot_header), symbol_label, TRUE, TRUE, 0);
        
        
        const char *status_text = bot->status == BOT_RUNNING ? "[>] RUNNING" :
                                 bot->status == BOT_PAUSED ? "[||] PAUSED" : "[X] STOPPED";
        const char *status_color = bot->status == BOT_RUNNING ? "#30d158" :
                                   bot->status == BOT_PAUSED ? "#ffd60a" : "#ff453a";
        
        GtkWidget *status_label = gtk_label_new(NULL);
        char status_markup[128];
        snprintf(status_markup, sizeof(status_markup),
                "<span foreground='%s'><b>%s</b></span>", status_color, status_text);
        gtk_label_set_markup(GTK_LABEL(status_label), status_markup);
        gtk_box_pack_end(GTK_BOX(bot_header), status_label, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(bot_card), bot_header, FALSE, FALSE, 0);
        
        
        bot_update_statistics(bot);
        double total_value = bot_get_total_value(bot, current_price);
        double roi = bot_get_roi(bot, current_price);
        
        GtkWidget *metrics_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        
        
        char balance_text[512];
        snprintf(balance_text, sizeof(balance_text),
                "<span size='small'>"
                "Initial: <b>$%.2f</b>  |  "
                "Balance: <b>$%.2f</b>  |  "
                "Position: <b>%.4f %s</b>  |  "
                "Total Value: <span foreground='%s'><b>$%.2f</b></span></span>",
                bot->initial_balance,
                bot->current_balance,
                bot->current_position, upper_symbol,
                roi >= 0 ? "#30d158" : "#ff453a",
                total_value);
        
        GtkWidget *balance_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(balance_label), balance_text);
        gtk_widget_set_halign(balance_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(metrics_box), balance_label, FALSE, FALSE, 0);
        
        
        char roi_text[512];
        snprintf(roi_text, sizeof(roi_text),
                "<span size='small'>"
                "ROI: <span foreground='%s'><b>%+.2f%%</b></span>  |  "
                "Net Profit: <span foreground='%s'><b>$%+.2f</b></span>  |  "
                "Fees Paid: <span foreground='#ff9500'><b>$%.2f</b></span></span>",
                roi >= 0 ? "#30d158" : "#ff453a", roi,
                bot->total_profit >= 0 ? "#30d158" : "#ff453a", bot->total_profit,
                bot->total_fees_paid);
        
        GtkWidget *roi_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(roi_label), roi_text);
        gtk_widget_set_halign(roi_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(metrics_box), roi_label, FALSE, FALSE, 0);
        
        
        char stats_text[512];
        snprintf(stats_text, sizeof(stats_text),
                "<span size='small'>"
                "Trades: <b>%d</b>  |  "
                "Wins: <span foreground='#30d158'><b>%d</b></span>  |  "
                "Losses: <span foreground='#ff453a'><b>%d</b></span>  |  "
                "Win Rate: <b>%.1f%%</b>  |  "
                "Max Drawdown: <span foreground='#ff453a'><b>%.1f%%</b></span></span>",
                bot->total_trades,
                bot->winning_trades,
                bot->losing_trades,
                bot->win_rate,
                bot->max_drawdown * 100.0);
        
        GtkWidget *stats_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(stats_label), stats_text);
        gtk_widget_set_halign(stats_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(metrics_box), stats_label, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(bot_card), metrics_box, FALSE, FALSE, 0);
        
        
        if (bot->trade_count > 0) {
            GtkWidget *trades_label = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(trades_label), "<b>Recent Trades:</b>");
            gtk_widget_set_halign(trades_label, GTK_ALIGN_START);
            gtk_widget_set_margin_top(trades_label, 4);
            gtk_box_pack_start(GTK_BOX(bot_card), trades_label, FALSE, FALSE, 0);
            
            int start_idx = bot->trade_count > 5 ? bot->trade_count - 5 : 0;
            int display_start = (bot->trade_index - (bot->trade_count - start_idx) + MAX_TRADES_HISTORY) % MAX_TRADES_HISTORY;
            
            for (int j = 0; j < (bot->trade_count > 5 ? 5 : bot->trade_count); j++) {
                int idx = (display_start + j) % MAX_TRADES_HISTORY;
                TradeRecord *trade = &bot->trades[idx];
                
                char trade_text[400];
                const char *action_color = strcmp(trade->action, "BUY") == 0 ? "#30d158" : "#ff9500";
                
                time_t now = time(NULL);
                int seconds_ago = (int)difftime(now, trade->timestamp);
                char time_str[64];
                if (seconds_ago < 60) {
                    snprintf(time_str, sizeof(time_str), "%ds ago", seconds_ago);
                } else if (seconds_ago < 3600) {
                    snprintf(time_str, sizeof(time_str), "%dm ago", seconds_ago / 60);
                } else {
                    snprintf(time_str, sizeof(time_str), "%dh ago", seconds_ago / 3600);
                }
                
                snprintf(trade_text, sizeof(trade_text),
                        "<span size='small' font_family='monospace'>"
                        "<span foreground='%s'><b>%s</b></span> %.4f @ $%.6f (fee: $%.4f) | %s | Balance: $%.2f"
                        "</span>",
                        action_color, trade->action,
                        trade->quantity, trade->price, trade->fee,
                        time_str, trade->balance_after);
                
                GtkWidget *trade_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(trade_label), trade_text);
                gtk_widget_set_halign(trade_label, GTK_ALIGN_START);
                gtk_label_set_line_wrap(GTK_LABEL(trade_label), TRUE);
                gtk_label_set_max_width_chars(GTK_LABEL(trade_label), 80);
                gtk_box_pack_start(GTK_BOX(bot_card), trade_label, FALSE, FALSE, 0);
            }
        }
        
        
        GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_top(button_box, 8);
        
        GtkWidget *start_btn = gtk_button_new_with_label("[>] Start");
        GtkWidget *pause_btn = gtk_button_new_with_label("[||] Pause");
        GtkWidget *stop_btn = gtk_button_new_with_label("[ ] Stop");
        GtkWidget *reset_btn = gtk_button_new_with_label("[R] Reset");
        GtkWidget *remove_btn = gtk_button_new_with_label("[X] Remove");
        
        
        if (bot->status == BOT_RUNNING) {
            gtk_widget_set_sensitive(start_btn, FALSE);
        } else if (bot->status == BOT_STOPPED) {
            gtk_widget_set_sensitive(pause_btn, FALSE);
            gtk_widget_set_sensitive(stop_btn, FALSE);
        } else if (bot->status == BOT_PAUSED) {
            gtk_widget_set_sensitive(pause_btn, FALSE);
        }
        
        
        BotButtonData *start_data = g_malloc(sizeof(BotButtonData));
        start_data->app = app;
        start_data->bot_index = i;
        
        BotButtonData *pause_data = g_malloc(sizeof(BotButtonData));
        pause_data->app = app;
        pause_data->bot_index = i;
        
        BotButtonData *stop_data = g_malloc(sizeof(BotButtonData));
        stop_data->app = app;
        stop_data->bot_index = i;
        
        BotButtonData *reset_data = g_malloc(sizeof(BotButtonData));
        reset_data->app = app;
        reset_data->bot_index = i;
        
        BotButtonData *remove_data = g_malloc(sizeof(BotButtonData));
        remove_data->app = app;
        remove_data->bot_index = i;
        
        
        g_signal_connect(start_btn, "clicked", G_CALLBACK(on_bot_start_clicked), start_data);
        g_signal_connect(pause_btn, "clicked", G_CALLBACK(on_bot_pause_clicked), pause_data);
        g_signal_connect(stop_btn, "clicked", G_CALLBACK(on_bot_stop_clicked), stop_data);
        g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_bot_reset_clicked), reset_data);
        g_signal_connect(remove_btn, "clicked", G_CALLBACK(on_bot_remove_clicked), remove_data);
        
        gtk_box_pack_start(GTK_BOX(button_box), start_btn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(button_box), pause_btn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(button_box), stop_btn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(button_box), reset_btn, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(button_box), remove_btn, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(bot_card), button_box, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(main_box), bot_card, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
    }
    
    if (active_bots == 0) {
        GtkWidget *no_bots = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(no_bots),
            "<span size='large'>No active bots</span>\n\n"
            "<span size='small'>Click 'Add New Bot' to create your first scalping bot!</span>");
        gtk_widget_set_halign(no_bots, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(no_bots, 40);
        gtk_widget_set_margin_bottom(no_bots, 40);
        gtk_box_pack_start(GTK_BOX(main_box), no_bots, TRUE, TRUE, 0);
    }
    
    gtk_container_add(GTK_CONTAINER(scrolled), main_box);
    gtk_container_add(GTK_CONTAINER(content), scrolled);
    
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
