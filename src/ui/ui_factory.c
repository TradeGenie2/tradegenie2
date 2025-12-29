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

#include "ui/ui_factory.h"
#include <stdlib.h>
#include <stdio.h>


UIInterface* ui_gtk_create(Portfolio *portfolio, NetworkManager *network,
                           const UICallbacks *callbacks, void *user_data);
void ui_gtk_destroy(UIInterface *ui);




UIInterface* ui_factory_create(UIType type, Portfolio *portfolio, NetworkManager *network,
                                const UICallbacks *callbacks, void *user_data) {
    switch (type) {
        case UI_TYPE_GTK:
            return ui_gtk_create(portfolio, network, callbacks, user_data);
        
        case UI_TYPE_COCOA:
            fprintf(stderr, "Cocoa UI not yet implemented\n");
            return NULL;
        
        case UI_TYPE_WINDOWS:
            fprintf(stderr, "Windows UI not yet implemented\n");
            return NULL;
        
        case UI_TYPE_QT:
            fprintf(stderr, "Qt UI not yet implemented\n");
            return NULL;
        
        default:
            fprintf(stderr, "Unknown UI type\n");
            return NULL;
    }
}

void ui_factory_destroy(UIInterface *ui) {
    if (!ui) return;
    
    if (ui->cleanup) {
        ui->cleanup(ui->impl_data);
    }
    
    free(ui);
}
