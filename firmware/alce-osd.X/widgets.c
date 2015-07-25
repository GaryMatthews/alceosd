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

/* all availabe widgets */
extern const struct widget_ops altitude_widget_ops;
extern const struct widget_ops bat_info_widget_ops;
extern const struct widget_ops compass_widget_ops;
extern const struct widget_ops flightmode_widget_ops;
extern const struct widget_ops gps_info_widget_ops;
extern const struct widget_ops home_info_widget_ops;
extern const struct widget_ops horizon_widget_ops;
extern const struct widget_ops radar_widget_ops;
extern const struct widget_ops rc_channels_widget_ops;
extern const struct widget_ops rssi_widget_ops;
extern const struct widget_ops speed_widget_ops;
extern const struct widget_ops throttle_widget_ops;
extern const struct widget_ops vario_graph_widget_ops;
extern const struct widget_ops wind_widget_ops;
extern const struct widget_ops flight_info_widget_ops;
extern const struct widget_ops console_widget_ops;


const struct widget_ops *all_widget_ops[] = {
    &altitude_widget_ops,
    &bat_info_widget_ops,
    &compass_widget_ops,
    &flightmode_widget_ops,
    &gps_info_widget_ops,
    &home_info_widget_ops,
    &horizon_widget_ops,
    &radar_widget_ops,
    &rc_channels_widget_ops,
    &rssi_widget_ops,
    &speed_widget_ops,
    &throttle_widget_ops,
    &vario_graph_widget_ops,
    &wind_widget_ops,
    &flight_info_widget_ops,
    &console_widget_ops,
    NULL,
};

#define WIDGET_FIFO_MASK    (0x3f)
#define MAX_WIDGET_ALLOC_MEM (0x1000)

struct widgets_mem_s {
    unsigned int mem[MAX_WIDGET_ALLOC_MEM/2];
    unsigned int alloc_size;
} widgets_mem = {
    .alloc_size = 0,
};

struct widget_fifo {
    struct widget *fifo[WIDGET_FIFO_MASK + 1];
    unsigned int rd;
    unsigned int wr;
} wfifo = {
    .rd = 0,
    .wr = 0,
};


/* custom memory allocator for widgets */
void* widget_malloc(unsigned int size)
{
    unsigned int *ptr;
    size = (size + 1) & 0xfffe;
    if ((widgets_mem.alloc_size + size) >= MAX_WIDGET_ALLOC_MEM)
        return NULL;

    ptr = &widgets_mem.mem[widgets_mem.alloc_size];
    widgets_mem.alloc_size += size;

    memset(ptr, 0, size);

    return ptr;
}


const struct widget_ops *get_widget_ops(unsigned int id)
{
    const struct widget_ops **w = all_widget_ops;

    while ((*w) != NULL) {
        if ((*w)->id == id)
            break;
        else
            w++;
    }
    return (*w);
}


static inline void render_widget(struct widget *w)
{
    if (init_canvas(&w->ca, 0) == 0) {
        w->ops->render(w);
        schedule_canvas(&w->ca);
    }
    w->status = 0;
}

extern volatile unsigned char sram_busy;

void widgets_process(void)
{
    struct widget *w;

    while (wfifo.rd != wfifo.wr) {
        w = wfifo.fifo[wfifo.rd++];
        wfifo.rd &= WIDGET_FIFO_MASK;
        render_widget(w);
        if (!sram_busy)
            break;
    }
}


void schedule_widget(struct widget *w)
{
    if (w->status == WIDGET_SCHEDULED)
        return;

    w->status = WIDGET_SCHEDULED;
    wfifo.fifo[wfifo.wr++] = w;
    wfifo.wr &= WIDGET_FIFO_MASK;
}


void widgets_reset(void)
{
    /* remove widget related timers/tasks */
    remove_timers(TIMER_WIDGET);
    /* remove widget related mavlink callbacks */
    del_mavlink_callbacks(CALLBACK_WIDGET);
    /* reset widget fifo */
    wfifo.rd = wfifo.wr = 0;
    /* reset widgets mem allocator */
    widgets_mem.alloc_size = 0;
    /* release all canvas memory */
    free_mem();
    /* clear display */
    clear_sram();
}


extern struct alceosd_config config;


enum {
    WID_PARAM_TAB = 0,
    WID_PARAM_X,
    WID_PARAM_Y,
    WID_PARAM_HJUST,
    WID_PARAM_VJUST,
    WID_PARAM_MODE,
    WID_PARAM_SOURCE,
    WID_PARAM_UNITS,
    WID_PARAM_PARAM1,
    WID_PARAM_PARAM2,
    WID_PARAM_PARAM3,
    WID_PARAM_PARAM4,
    WID_PARAM_END
};

const char widget_param_lut[WID_PARAM_END][10] = {
    [WID_PARAM_TAB] = "TAB",
    [WID_PARAM_X] = "X",
    [WID_PARAM_Y] = "Y",
    [WID_PARAM_HJUST] = "HJUST",
    [WID_PARAM_VJUST] = "VJUST",
    [WID_PARAM_MODE] = "MODE",
    [WID_PARAM_SOURCE] = "SOURCE",
    [WID_PARAM_UNITS] = "UNITS",
    [WID_PARAM_PARAM1] = "PARAM1",
    [WID_PARAM_PARAM2] = "PARAM2",
    [WID_PARAM_PARAM3] = "PARAM3",
    [WID_PARAM_PARAM4] = "PARAM4",
};


/* appends a param to the name */
static void get_widget_param(unsigned int pidx,
            struct widget_config *wcfg, struct mavlink_param *p)
{
    struct mavlink_param_value *pv = p->value;
    if (pidx >= WID_PARAM_END)
        return;

    strncat(p->name, widget_param_lut[pidx], 8);
    switch (pidx) {
        case WID_PARAM_TAB:
            p->type = MAV_PARAM_TYPE_UINT8;
            pv->param_uint8 = wcfg->tab;
            break;
        case WID_PARAM_X:
            p->type = MAV_PARAM_TYPE_INT16;
            pv->param_int16 = wcfg->x;
            break;
        case WID_PARAM_Y:
            p->type = MAV_PARAM_TYPE_INT16;
            pv->param_int16 = wcfg->y;
            break;
        case WID_PARAM_VJUST:
            p->type = MAV_PARAM_TYPE_UINT8;
            pv->param_uint8 = wcfg->props.vjust;
            break;
        case WID_PARAM_HJUST:
            p->type = MAV_PARAM_TYPE_UINT8;
            pv->param_uint8 = wcfg->props.hjust;
            break;
        case WID_PARAM_MODE:
            p->type = MAV_PARAM_TYPE_UINT8;
            pv->param_uint8 = wcfg->props.mode;
            break;
        case WID_PARAM_UNITS:
            p->type = MAV_PARAM_TYPE_UINT8;
            pv->param_uint8 = wcfg->props.units;
            break;
        case WID_PARAM_SOURCE:
            p->type = MAV_PARAM_TYPE_UINT8;
            pv->param_uint8 = wcfg->props.source;
            break;
        case WID_PARAM_PARAM1:
        case WID_PARAM_PARAM2:
        case WID_PARAM_PARAM3:
        case WID_PARAM_PARAM4:
            p->type = MAV_PARAM_TYPE_UINT16;
            pv->param_uint16 = wcfg->params[pidx - WID_PARAM_PARAM1];
            break;
        default:
            break;
    }

}

static void set_widget_param(struct widget_config *wcfg, struct mavlink_param *p)
{
    char pname[10];
    unsigned char i;
    struct mavlink_param_value *pv = p->value;
    
    strcpy(pname, &p->name[7]);
    //console_printf("pname=%s\n", pname);

    for(i = 0; i < WID_PARAM_END; i++) {
        if (strcmp(pname, widget_param_lut[i]) == 0)
            break;
    }

    if (i == WID_PARAM_END) {
        console_printf("lut failed\n");
        return;
    }

    console_printf("pidx=%d\n", i);

    switch (i) {
        case WID_PARAM_TAB:
            wcfg->tab = pv->param_uint8;
            break;
        case WID_PARAM_X:
            wcfg->x = pv->param_int16;
            break;
        case WID_PARAM_Y:
            wcfg->y = pv->param_int16;
            break;
        case WID_PARAM_VJUST:
            wcfg->props.vjust = pv->param_uint8;
            break;
        case WID_PARAM_HJUST:
            wcfg->props.hjust = pv->param_uint8;
            break;
        case WID_PARAM_MODE:
            wcfg->props.mode = pv->param_uint8;
            break;
        case WID_PARAM_UNITS:
            wcfg->props.units = pv->param_uint8;
            break;
        case WID_PARAM_SOURCE:
            wcfg->props.source = pv->param_uint8;
            break;
        case WID_PARAM_PARAM1:
        case WID_PARAM_PARAM2:
        case WID_PARAM_PARAM3:
        case WID_PARAM_PARAM4:
            wcfg->params[i - WID_PARAM_PARAM1] = pv->param_uint16;
            break;
    }


}

static unsigned int count_widget_params(void)
{
    struct widget_config *wcfg = config.widgets;
    unsigned int total = 0;

    while ((wcfg++)->tab != TABS_END) {
        total++;
    }

    return WID_PARAM_END * total;
}


static void get_widget_params(int idx, struct mavlink_param *p)
{
    struct widget_config *wcfg;
    const struct widget_ops *wops;
    unsigned int table_idx = idx / WID_PARAM_END;
    unsigned int param_idx = idx % WID_PARAM_END;

    unsigned int table_max = count_widget_params() / WID_PARAM_END;

    if (table_idx >= table_max)
        return;

    /* get widget at location */
    wcfg = &config.widgets[table_idx];
    wops = get_widget_ops(wcfg->widget_id);

    sprintf(p->name, "W%1d", wcfg->uid);
    strncat(p->name, wops->name, 4);
    strcat(p->name, "_");

    get_widget_param(param_idx, wcfg, p);
}

static void set_widget_params(struct mavlink_param *p)
{
    struct widget_config *wcfg = config.widgets;
    const struct widget_ops *wops;

    unsigned char uid;
    char buf[4];

    uid = p->name[1] - '0';
    memcpy(buf, &p->name[2], 4);
    while (wcfg->tab != TABS_END) {
        wops = get_widget_ops(wcfg->widget_id);
        if ((uid == wcfg->uid) && (memcmp(buf, wops->name, 4) == 0)) {
            set_widget_param(wcfg, p);
        }
        wcfg++;
    }
}



struct mavlink_dynamic_param_def mavparams_widgets = {
    .get = get_widget_params,
    .set = set_widget_params,
    .count = count_widget_params,
};


void widgets_init(void)
{
    const struct widget_ops **w = all_widget_ops;

    mavlink_set_dynamic_params(&mavparams_widgets);

    while ((*w) != NULL) {
        if ((*w)->init != NULL)
            (*w)->init();
        w++;
    }
}
