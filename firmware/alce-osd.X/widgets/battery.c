/*
    AlceOSD - Graphical OSD
    Copyright (C) 2015  Luis Alves

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "alce-osd.h"

#define BAT_BAR_X   3
#define BAT_BAR_Y   0
#define BAT_BAR_W   58
#define BAT_BAR_H   14


struct widget_priv {
    float bat_voltage, bat_current;
    int bat_remaining;

    unsigned int *adc_raw, *adc_raw2;
    float bat_voltage2;
};


static void mav_callback(struct timer *t, void *d) //(mavlink_message_t *msg, void *d)
{
    struct widget *w = d;
    struct widget_priv *priv = w->priv;
    
    unsigned long p;
    
    mavlink_sys_status_t *s = mavdata_get(MAVLINK_MSG_ID_SYS_STATUS);
    
    priv->bat_voltage = s->voltage_battery / 1000.0;
    priv->bat_current = s->current_battery / 100.0;
    priv->bat_remaining = (int) s->battery_remaining;

    
    p = mavdata_period(MAVLINK_MSG_ID_SYS_STATUS);
    p = max(p, 200);
    p = min(p, 1000);
    set_timer_period(t, p);
    
    schedule_widget(w);
}

static void timer_callback(struct timer *t, void *d)
{
    struct widget *w = d;
    struct widget_priv *priv = w->priv;

    priv->bat_voltage = *(priv->adc_raw) * 18.3 / (1 << 10);
    priv->bat_voltage2 = *(priv->adc_raw2) * 18.3 / (1 << 10);
    schedule_widget(w);
}

static void current_tmr_callback(struct timer *t, void *d)
{
    struct widget *w = d;
    struct widget_priv *priv = w->priv;
    struct flight_stats *f = get_flight_stats();

    priv->bat_current = f->total_mah;
    schedule_widget(w);
}

static int open(struct widget *w)
{
    struct widget_priv *priv;

    priv = (struct widget_priv*) widget_malloc(sizeof(struct widget_priv));
    if (priv == NULL)
        return -1;
    w->priv = priv;
    w->ca.width = 64;

    switch (w->cfg->props.mode) {
        default:
        case 0:
            add_timer(TIMER_WIDGET, 500, mav_callback, w);
            //add_mavlink_callback(MAVLINK_MSG_ID_SYS_STATUS, mav_callback, CALLBACK_WIDGET, w);
            w->ca.height = 45;
            break;
        case 1:
            /* adc ch 0 */
            adc_start(100);
            adc_link_ch(0, &priv->adc_raw);
            w->ca.height = 15;
            add_timer(TIMER_WIDGET, 200, timer_callback, w);
            break;
        case 2:
            /* adc ch 1 */
            adc_start(100);
            adc_link_ch(1, &priv->adc_raw);
            w->ca.height = 15;
            add_timer(TIMER_WIDGET, 200, timer_callback, w);
            break;
        case 3:
            /* adc ch 0,1 */
            adc_start(100);
            adc_link_ch(0, &priv->adc_raw);
            adc_link_ch(1, &priv->adc_raw2);
            w->ca.height = 30;
            add_timer(TIMER_WIDGET, 200, timer_callback, w);
            break;
        case 4:
            /* flight total current consumption */
            w->ca.height = 15;
            add_timer(TIMER_WIDGET, 200, current_tmr_callback, w);
            break;
    }

    return 0;
}


static void close(struct widget *w)
{
    adc_stop();
}


static void render(struct widget *w)
{
    struct widget_priv *priv = w->priv;
    struct canvas *ca = &w->ca;
    char buf[20];
    int i;


    switch (w->cfg->props.mode) {
        case 0:
        default:
            sprintf(buf, "%5.2fV\n%5.2fA", priv->bat_voltage, priv->bat_current);
            draw_jstr(buf, BAT_BAR_X + BAT_BAR_W, BAT_BAR_Y + BAT_BAR_H, JUST_RIGHT, ca, 2);

            draw_rect(BAT_BAR_X, BAT_BAR_Y, BAT_BAR_X + BAT_BAR_W, BAT_BAR_Y + BAT_BAR_H, 3, ca);
            draw_rect(BAT_BAR_X+1, BAT_BAR_Y+1, BAT_BAR_X + BAT_BAR_W-1, BAT_BAR_Y + BAT_BAR_H-1, 1, ca);
            draw_vline(BAT_BAR_X - 1, BAT_BAR_Y + 4, BAT_BAR_Y + BAT_BAR_H - 4, 1, ca);
            draw_vline(BAT_BAR_X - 2, BAT_BAR_Y + 4, BAT_BAR_Y + BAT_BAR_H - 4, 1, ca);
            draw_vline(BAT_BAR_X - 3, BAT_BAR_Y + 4, BAT_BAR_Y + BAT_BAR_H - 4, 3, ca);

            for (i = 0; i < (priv->bat_remaining*BAT_BAR_W)/100 - 2; i++) {
                draw_vline(BAT_BAR_X + 2 + i, BAT_BAR_Y + 2, BAT_BAR_Y + BAT_BAR_H - 2, 2, ca);
            }

            sprintf(buf, "%d%%", priv->bat_remaining);
            draw_jstr(buf, BAT_BAR_X + BAT_BAR_W/2, BAT_BAR_Y+BAT_BAR_H/2,
                            JUST_VCENTER | JUST_HCENTER, ca, 0);
            break;
        case 1:
        case 2:
            sprintf(buf, "%5.2fV", priv->bat_voltage);
            draw_str(buf, 0, 0, ca, 2);
            break;
        case 3:
            sprintf(buf, "%5.2fV\n%5.2fV", priv->bat_voltage, priv->bat_voltage2);
            draw_str(buf, 0, 0, ca, 2);
            break;
        case 4:
            sprintf(buf, "%4dmAh", (unsigned int) priv->bat_current);
            draw_str(buf, 0, 0, ca, 2);
            break;
    }
}


const struct widget_ops bat_info_widget_ops = {
    .name = "Battery info",
    .mavname = "BATTERY",
    .id = WIDGET_BATTERY_INFO_ID,
    .init = NULL,
    .open = open,
    .render = render,
    .close = close,
};
