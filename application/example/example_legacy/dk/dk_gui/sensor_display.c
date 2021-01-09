/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <fcntl.h>

#include "aos/kernel.h"

#include "lvgl.h"
#include <k_api.h>
#include "sensor_display.h"
#include "sensor/sensor.h"
#include "st7789.h"
#include "soc_init.h"

LV_IMG_DECLARE(AliOS_Things_logo);

#define MAX_MSG_BYTES 100
#define MAX_NUM_BYTES 20

char msg_buffer[MAX_MSG_BYTES];
char number_buf[MAX_NUM_BYTES];

/* gui object definition */
lv_obj_t *scr     = NULL;
lv_obj_t *img_src = NULL;

lv_obj_t *lmeter1 = NULL;
lv_obj_t *lmeter2 = NULL;
lv_obj_t *lmeter3 = NULL;
lv_obj_t *lmeter4 = NULL;

lv_obj_t *label1     = NULL;
lv_obj_t *label2     = NULL;
lv_obj_t *label3     = NULL;
lv_obj_t *label4     = NULL;
lv_obj_t *label5     = NULL;
lv_obj_t *label6     = NULL;
lv_obj_t *label7     = NULL;
lv_obj_t *label8     = NULL;
lv_obj_t *label_note = NULL;

lv_obj_t *         chart1 = NULL;
lv_chart_series_t *dl1_1  = NULL;
lv_chart_series_t *dl1_2  = NULL;
lv_chart_series_t *dl1_3  = NULL;

lv_obj_t *calendar = NULL;

aos_timer_t refresh_timer;

/* sensor fd */
int fd_acc  = -1;
int fd_temp = -1;
int fd_humi = -1;
int fd_baro = -1;

int key_pressed_cnt      = 0;
int weather_create_flag  = 0;
int calendar_create_flag = 0;
int acc_create_flag      = 0;

static lv_style_t style;
static lv_style_t style1;
static lv_style_t style2;
static lv_style_t style3;

static void littlevgl_refresh_task(void *arg);
static void freetype_display(void);
static void app_init(void);
static void key_init(void);

static void logo_display(void);
static void sensor_data_display(void);
static void sensor_refresh_task(void *arg);

static void weather_display(void);
static void create_weather(void);
static void refresh_weather(void);

static void calendar_display(void);
static void create_calendar(void);
static void refresh_calendar(void);

static void acc_display(void);
static void create_acc(void);
static void refresh_acc(void);

static void display_note(void);
static void clean_screen(void);

static int get_acc_data(float *x, float *y, float *z);
static int get_temperature_data(float *dataT);
static int get_Humidity_data(float *dataH);
static int get_barometer_data(float *dataP);
static int get_noise_data(float *data);
static int get_CH2O_data(float *data);
static int get_CO2_data(float *data);
static int get_PM2d5_data(float *data);

void sensor_display(void)
{
    /* key init */
    key_init();

    /* open acc sensor */
    fd_acc = aos_open(DEV_ACC_PATH(0), O_RDWR);

    if (fd_acc < 0) {
        printf("acc sensor open failed !\n");
    }

    /* open temp sensor */
    fd_temp = aos_open(DEV_TEMP_PATH(0), O_RDWR);

    if (fd_temp < 0) {
        printf("temp sensor open failed !\n");
    }

    /* open temp sensor */
    fd_humi = aos_open(DEV_HUMI_PATH(0), O_RDWR);

    if (fd_humi < 0) {
        printf("humi sensor open failed !\n");
    }

    fd_baro = aos_open(DEV_BARO_PATH(0), O_RDWR);

    if (fd_baro < 0) {
        printf("baro sensor open failed !\n");
    }

    /* create a timer to refresh sensor data */
    aos_timer_new(&refresh_timer, sensor_refresh_task, NULL, 800, 1);
}

static void sensor_refresh_task(void *arg)
{
    static int task_count = 0;

    /* disaply alios logo */
    if (task_count == 0) {
        scr = lv_scr_act();

        logo_display();
    }

    /* hide alios logo and display sensor data */
    if (task_count == 5) {
        lv_obj_clean(scr);
    }

    /* refresh sensor data */
    if (task_count >= 5) {
        sensor_data_display();
    }

    if ((task_count >= 5) && (task_count <= 25)) {
        //display_note();
    }

    task_count++;
}

static void logo_display(void)
{
    img_src = lv_img_create(scr, NULL); /*Crate an image object*/
    lv_img_set_src(img_src, &AliOS_Things_logo);
    lv_obj_set_pos(img_src, 60, 60); /*Set the positions*/
    lv_obj_set_drag(img_src, true);
}

static void sensor_data_display(void)
{
    if (key_pressed_cnt % 3 == 0) {
        if (weather_create_flag == 0) {
            clean_screen();
        }

        weather_display();
    } else if (key_pressed_cnt % 3 == 1) {

        if (calendar_create_flag == 0) {
            clean_screen();
        }

        calendar_display();
    } else {

        if (acc_create_flag == 0) {
            clean_screen();
        }

        acc_display();
    }
}

static void weather_display(void)
{
    if (weather_create_flag == 0) {
        create_weather();
        weather_create_flag = 1;
    }

    if (weather_create_flag == 1) {
        refresh_weather();
    }
}

static void calendar_display(void)
{
    if (calendar_create_flag == 0) {
        create_calendar();
        calendar_create_flag = 1;
    }

    if (calendar_create_flag == 1) {
        refresh_calendar();
    }
}

static void acc_display(void)
{
    if (acc_create_flag == 0) {
        create_acc();
        acc_create_flag = 1;
    }

    if (acc_create_flag == 1) {
        refresh_acc();
    }
}

static void create_weather(void)
{
    static lv_style_t style_lmeter1;
    lv_style_copy(&style_lmeter1, &lv_style_pretty_color);
    style_lmeter1.line.width      = 2;
    style_lmeter1.line.color      = LV_COLOR_YELLOW;
    style_lmeter1.body.main_color = LV_COLOR_RED;  /*Light blue*/
    style_lmeter1.body.grad_color = LV_COLOR_PURPLE; /*Dark blue*/

    /*Create the first line meter */
    lmeter1 = lv_lmeter_create(scr, NULL);
    lv_lmeter_set_range(lmeter1, 20, 50);         /*Set the range*/
    lv_lmeter_set_value(lmeter1, 30);             /*Set the current value*/
    lv_lmeter_set_style(lmeter1, 0, &style_lmeter1); /*Apply the new style*/
    lv_obj_set_size(lmeter1, 100, 100);
    lv_obj_set_pos(lmeter1, 10, 15);

    /*Add a label to show the current value*/
    label1 = lv_label_create(lmeter1, NULL);
    lv_label_set_text(label1, "0");
    lv_style_copy(&style, &lv_style_plain);
    style.text.color = LV_COLOR_PURPLE;

    lv_label_set_style(label1, 0, &style);
    lv_obj_align(label1, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_pos(label1, 25, 45);

    label5 = lv_label_create(scr, label1);
    lv_obj_set_pos(label5, 10, 105);
    lv_label_set_text(label5, "Temp(C)");

    /*Create the first line meter */
    lmeter2 = lv_lmeter_create(scr, NULL);
    lv_lmeter_set_range(lmeter2, 0, 100);         /*Set the range*/
    lv_lmeter_set_value(lmeter2, 30);             /*Set the current value*/
    lv_lmeter_set_style(lmeter2, 0, &style_lmeter1); /*Apply the new style*/
    lv_obj_set_size(lmeter2, 100, 100);
    lv_obj_set_pos(lmeter2, 130, 15);

    /*Add a label to show the current value*/
    label2 = lv_label_create(lmeter2, label1);
    lv_label_set_text(label2, "0");
    lv_obj_align(label2, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_pos(label2, 25, 45);

    label6 = lv_label_create(scr, label1);
    lv_obj_set_pos(label6, 130, 105);
    lv_label_set_text(label6, "Humidity(%)");

    /*Create the first line meter */
    lmeter3 = lv_lmeter_create(scr, NULL);
    lv_lmeter_set_range(lmeter3, 95, 110);        /*Set the range*/
    lv_lmeter_set_value(lmeter3, 30);             /*Set the current value*/
    lv_lmeter_set_style(lmeter3, 0, &style_lmeter1); /*Apply the new style*/
    lv_obj_set_size(lmeter3, 100, 100);
    lv_obj_set_pos(lmeter3, 10, 130);

    /*Add a label to show the current value*/
    label3 = lv_label_create(lmeter3, label1);
    lv_label_set_text(label3, "0");
    lv_obj_align(label3, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_pos(label3, 25, 40);

    label7 = lv_label_create(scr, label1);
    lv_obj_set_pos(label7, 10, 220);
    lv_label_set_text(label7, "Barometer(Pa)");

    /*Create the first line meter */
    lmeter4 = lv_lmeter_create(scr, NULL);
    lv_lmeter_set_range(lmeter4, 0, 100);         /*Set the range*/
    lv_lmeter_set_value(lmeter4, 30);             /*Set the current value*/
    lv_lmeter_set_style(lmeter4, 0, &style_lmeter1); /*Apply the new style*/
    lv_obj_set_size(lmeter4, 100, 100);
    lv_obj_set_pos(lmeter4, 130, 130);

    /*Add a label to show the current value*/
    label4 = lv_label_create(lmeter4, label1);
    lv_label_set_text(label4, "0");
    lv_obj_align(label4, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_pos(label4, 25, 40);

    label8 = lv_label_create(scr, label1);
    lv_obj_set_pos(label8, 150, 220);
    lv_label_set_text(label8, "Noise(DB)");

    label_note = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label_note, LV_LABEL_LONG_SROLL_CIRC);     /*Circular scroll*/
    lv_obj_set_width(label_note, 230);
    lv_obj_set_pos(label_note, 5, 0);
    lv_label_set_text(label_note, "Press any button to switch the screen !");
}

static void event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        lv_calendar_date_t * date = lv_calendar_get_pressed_date(obj);
        if(date) {
            lv_calendar_set_today_date(obj, date);
        }
    }
}

static void create_calendar(void)
{
    calendar = lv_calendar_create(lv_scr_act(), NULL);
    lv_obj_set_size(calendar, 230, 230);
    lv_obj_align(calendar, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_event_cb(calendar, event_handler);

    /*Set the today*/
    lv_calendar_date_t today;
    today.year = 2018;
    today.month = 10;
    today.day = 23;

    lv_calendar_set_today_date(calendar, &today);
    lv_calendar_set_showed_date(calendar, &today);

    /*Highlight some days*/
    static lv_calendar_date_t highlihted_days[3];       /*Only it's pointer will be saved so should be static*/
    highlihted_days[0].year = 2018;
    highlihted_days[0].month = 10;
    highlihted_days[0].day = 6;

    highlihted_days[1].year = 2018;
    highlihted_days[1].month = 10;
    highlihted_days[1].day = 11;

    highlihted_days[2].year = 2018;
    highlihted_days[2].month = 11;
    highlihted_days[2].day = 22;

    lv_calendar_set_highlighted_dates(calendar, highlihted_days, 3);
}

static void create_acc(void)
{
    /* create a chart to display all data */
    chart1 = lv_chart_create(scr, NULL);
    lv_chart_set_type(chart1, LV_CHART_TYPE_LINE);
    lv_style_copy(&style, &lv_style_pretty);
    style.body.shadow.width = 6;
    style.body.shadow.color = LV_COLOR_GRAY;
    style.line.color        = LV_COLOR_GRAY;
    style.body.main_color   = LV_COLOR_BLACK;
    style.body.grad_color   = LV_COLOR_BLACK;
    lv_obj_set_style(chart1, &style);
    lv_obj_set_size(chart1, 230, 145);
    lv_obj_set_pos(chart1, 5, 25);
    lv_chart_set_range(chart1, 0, 40);
    lv_chart_set_point_count(chart1, 50);
    lv_chart_set_div_line_count(chart1, 10, 8);

    /* add acc_x acc_y acc_z to the chart */
    dl1_1 = lv_chart_add_series(chart1, LV_COLOR_WHITE);
    dl1_2 = lv_chart_add_series(chart1, LV_COLOR_YELLOW);
    dl1_3 = lv_chart_add_series(chart1, LV_COLOR_ORANGE);

    lv_obj_t *obj1;
    obj1 = lv_obj_create(scr, NULL);
    lv_obj_set_size(obj1, 240, 65);
    lv_obj_set_style(obj1, &lv_style_plain_color);
    lv_obj_align(obj1, NULL, LV_ALIGN_CENTER, 0, 85);

    /* create a label to display acc_x */
    label1 = lv_label_create(scr, NULL);
    lv_obj_set_pos(label1, 10, 190);
    lv_style_copy(&style1, &lv_style_plain);
    style1.text.color = LV_COLOR_WHITE;
    lv_label_set_style(label1, 0, &style1);
    lv_obj_set_top(label1, 1);

    /* create a label to display acc_y */
    label2 = lv_label_create(scr, NULL);
    lv_obj_set_pos(label2, 10, 210);
    lv_style_copy(&style2, &lv_style_plain);
    style2.text.color = LV_COLOR_WHITE;
    lv_label_set_style(label2, 0, &style2);
    lv_obj_set_top(label2, 1);

    /* create a label to display acc_z */
    label3 = lv_label_create(scr, NULL);
    lv_obj_set_pos(label3, 10, 230);
    lv_style_copy(&style3, &lv_style_plain);
    style3.text.color = LV_COLOR_WHITE;
    lv_label_set_style(label3, 0, &style3);
    lv_obj_set_top(label3, 1);

    lv_obj_t *label_note2 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label_note2, LV_LABEL_LONG_SROLL_CIRC);     /*Circular scroll*/
    lv_obj_set_width(label_note2, 230);
    lv_obj_set_pos(label_note2, 5, 0);
    lv_label_set_text(label_note2, "Shake the board and you can see the graph of the acc !");
}

static void clean_screen(void)
{
    lv_obj_clean(scr);
}

static void refresh_weather(void)
{
    float temp  = 0;
    float humi  = 0;
    float baro  = 0;
    float noise = 0;
    int   ret   = -1;

    /* get temp sensor data */
    ret = get_temperature_data(&temp);
    if (ret != 0) {
        printf("temp sensor read failed !\n");
        return;
    }

    /* get humi sensor data */
    ret = get_Humidity_data(&humi);
    if (ret != 0) {
        printf("temp sensor read failed !\n");
        return;
    }

    /* get baro sensor data */
    ret = get_barometer_data(&baro);
    if (ret != 0) {
        printf("baro sensor read failed !\n");
        return;
    }

    /* get noise sensor data */
    ret = get_noise_data(&noise);
    if (ret != 0) {
        printf("noise sensor read failed !\n");
        return;
    }

    /* refresh label */
    sprintf(number_buf, "%.2f", temp);
    lv_label_set_text(label1, number_buf);

    sprintf(number_buf, "%.2f", humi);
    lv_label_set_text(label2, number_buf);

    sprintf(number_buf, "%.2f", baro);
    lv_label_set_text(label3, number_buf);

    sprintf(number_buf, "%.2f", noise);
    lv_label_set_text(label4, number_buf);

    lv_lmeter_set_value(lmeter1, (int16_t)temp);
    lv_lmeter_set_value(lmeter2, (int16_t)humi);
    lv_lmeter_set_value(lmeter3, (int16_t)baro);
    lv_lmeter_set_value(lmeter4, (int16_t)temp);
}

static void refresh_calendar(void)
{
    static int day = 1;

    /*Set the today*/
    lv_calendar_date_t today;
    today.year = 2018;
    today.month = 10;
    today.day = day;

    lv_calendar_set_today_date(calendar, &today);
    lv_calendar_set_showed_date(calendar, &today);

    day++;

    if (day > 31) {
        day = 1;
    }
}

static void refresh_acc(void)
{
    int        i       = 0;
    static int count   = 0;
    float      acc_x_f = 0;
    float      acc_y_f = 0;
    float      acc_z_f = 0;
    int        ret     = -1;

    /* get acc sensor data */
    ret = get_acc_data(&acc_x_f, &acc_y_f, &acc_z_f);
    if (ret != 0) {
        printf("acc sensor read failed !\n");
        return;
    }

    /* refresh chart data */
    if (count == 0) {
        /* to prevent sensor data from dithering at the first time */
        for (i = 0; i <= 49; i++) {
            dl1_1->points[i] = acc_x_f + 20;
            dl1_2->points[i] = acc_y_f + 20;
            dl1_3->points[i] = acc_z_f + 20;
        }
    } else {
        for (i = 48; i >= 0; i--) {
            dl1_1->points[i + 1] = dl1_1->points[i];
            dl1_2->points[i + 1] = dl1_2->points[i];
            dl1_3->points[i + 1] = dl1_3->points[i];
        }

        dl1_1->points[0] = acc_x_f + 20;
        dl1_2->points[0] = acc_y_f + 20;
        dl1_3->points[0] = acc_z_f + 20;
    }

    /* refresh chart */
    lv_chart_refresh(chart1);

    /* refresh label */
    sprintf(number_buf, "%.4f", acc_x_f);
    strcpy(msg_buffer, "acc_x: ");
    strcat(msg_buffer, number_buf);
    strcat(msg_buffer, "m/s2");

    lv_label_set_text(label1, msg_buffer);
    lv_obj_align(label1, NULL, LV_ALIGN_CENTER, 0, 65);

    sprintf(number_buf, "%.4f", acc_y_f);
    strcpy(msg_buffer, "acc_y: ");
    strcat(msg_buffer, number_buf);
    strcat(msg_buffer, "m/s2");

    lv_label_set_text(label2, msg_buffer);
    lv_obj_align(label2, NULL, LV_ALIGN_CENTER, 0, 85);

    sprintf(number_buf, "%.4f", acc_z_f);
    strcpy(msg_buffer, "acc_z: ");
    strcat(msg_buffer, number_buf);
    strcat(msg_buffer, "m/s2");

    lv_label_set_text(label3, msg_buffer);
    lv_obj_align(label3, NULL, LV_ALIGN_CENTER, 0, 105);

    count++;
}

static int get_acc_data(float *x, float *y, float *z)
{
    accel_data_t data = { 0 };
    ssize_t      size = 0;

    size = aos_read(fd_acc, &data, sizeof(data));
    if (size != sizeof(data)) {
        return -1;
    }

    *x = ((float)data.data[0] / 1000.0f) * 9.8f;
    *y = ((float)data.data[1] / 1000.0f) * 9.8f;
    *z = ((float)data.data[2] / 1000.0f) * 9.8f;

    return 0;
}

static int get_temperature_data(float *dataT)
{
    temperature_data_t data = { 0 };
    ssize_t            size = 0;

    size = aos_read(fd_temp, &data, sizeof(data));
    if (size != sizeof(data)) {
        return -1;
    }

    *dataT = (float)data.t / 10;

    return 0;
}

static int get_Humidity_data(float *dataH)
{
    humidity_data_t data = { 0 };
    ssize_t         size = 0;

    size = aos_read(fd_humi, &data, sizeof(data));
    if (size != sizeof(data)) {
        return -1;
    }

    *dataH = (float)data.h / 10;

    return 0;
}

static int get_barometer_data(float *dataP)
{
    barometer_data_t data = { 0 };
    ssize_t          size = 0;

    size = aos_read(fd_baro, &data, sizeof(data));
    if (size != sizeof(data)) {
        return -1;
    }

    *dataP = (float)data.p / 1000;

    return 0;
}

static int get_noise_data(float *data)
{
    static float cnt = 0.0f;

    if (cnt > 10.0f) {
        cnt = 0.0f;
    }

    *data = 25 + 0.1f * cnt;

    cnt += 1;

    return 0;
}

static int get_CH2O_data(float *data)
{
    static float cnt = 0.0f;

    if (cnt > 10.0f) {
        cnt = 0.0f;
    }

    *data = 0.36 + 0.01f * cnt;

    cnt += 1;

    return 0;
}

static int get_CO2_data(float *data)
{
    static float cnt = 0.0f;

    if (cnt > 10.0f) {
        cnt = 0.0f;
    }

    *data = 537 + cnt;

    cnt += 1;

    return 0;
}

static int get_PM2d5_data(float *data)
{
    static float cnt = 0.0f;

    if (cnt > 10.0f) {
        cnt = 0.0f;
    }

    *data = 8.5f + 0.1f * cnt;

    cnt += 1;

    return 0;
}

void key1_handle(void)
{
    key_pressed_cnt += 1;
    weather_create_flag = 0;
    calendar_create_flag   = 0;
    acc_create_flag     = 0;
}

void key2_handle(void)
{
    key_pressed_cnt += 1;
    weather_create_flag = 0;
    calendar_create_flag   = 0;
    acc_create_flag     = 0;
}

void key3_handle(void)
{
    key_pressed_cnt += 1;
    weather_create_flag = 0;
    calendar_create_flag   = 0;
    acc_create_flag     = 0;
}

void key_init(void)
{
    int ret = 0;

    ret |= hal_gpio_enable_irq(&brd_gpio_table[GPIO_KEY_1],
                               IRQ_TRIGGER_RISING_EDGE, key1_handle, NULL);
    ret |= hal_gpio_enable_irq(&brd_gpio_table[GPIO_KEY_2],
                               IRQ_TRIGGER_RISING_EDGE, key2_handle, NULL);
    ret |= hal_gpio_enable_irq(&brd_gpio_table[GPIO_KEY_3],
                               IRQ_TRIGGER_RISING_EDGE, key3_handle, NULL);
    if (ret != 0) {
        printf("hal_gpio_enable_irq key return failed.\n");
    }
}
