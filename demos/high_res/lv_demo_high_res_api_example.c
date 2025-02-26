/**
 * @file lv_demo_high_res_api_example.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include <stdlib.h>
#include <signal.h>
#include "../../src/core/lv_refr.h"
#include "lv_demo_high_res.h"
#include "../../src/osal/lv_os.h"
#include <string.h>
#include <stdio.h>
#include<pthread.h>
#if LV_USE_DEMO_HIGH_RES

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static pthread_t audio_thread;
static pthread_t mqtt_sub_thread;
static pthread_t clock_thread;
static pthread_t button_thread;
static pthread_t led_thread;
static pthread_t adc_thread;
static void exit_cb(int sig);
static void output_subject_observer_cb(lv_observer_t * observer, lv_subject_t * subject);

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_demo_high_res_api_t * api = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL VARIABLES
 **********************/
int delay_millis = 1000;
pthread_mutex_t delay_lock;
pthread_mutex_t playing_now_lock;
int playing_now=0;
extern int button_configured;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
extern void *audio_play(void *);
extern void *mqtt_sub_init(void *);
extern void *clock_init(void * );
extern void *button_init(void *);
extern void *led_blink(void *);
extern void *adc_init(void *);


void lv_demo_high_res_api_example(const char * assets_path, const char * logo_path, const char * slides_path)
{
	if (!api)
		api = lv_demo_high_res(assets_path, logo_path, slides_path, exit_cb);
	else
		return;

	/* handle SIGINT (for eg, sent by Crtl-C and `kill`) and SIGTERM (for eg, sent by `systemctl stop`) */
	struct sigaction sa;
	sa.sa_handler = exit_cb;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

    /* see lv_demo_high_res.h for documentation of the available subjects */
    lv_subject_set_int(&api->subjects.volume, 90);
    int error=0;
    char command[50];
    sprintf(command, "amixer set PCM %d\%\t", 90);
    error = system(command);
    lv_subject_set_int(&api->subjects.volume, 90);
    lv_subject_set_pointer(&api->subjects.month_name, "August");
    lv_subject_set_int(&api->subjects.month_day, 1);
    lv_subject_set_pointer(&api->subjects.week_day_name, "Wednesday");
    lv_subject_set_int(&api->subjects.temperature_outdoor, 16 * 10); /* 16 degrees C */
    lv_subject_set_int(&api->subjects.main_light_temperature, 4500); /* 4500 degrees K */
    lv_subject_set_int(&api->subjects.thermostat_target_temperature, 25 * 10); /* 25 degrees C */
    lv_subject_set_pointer(&api->subjects.wifi_ssid, "my home Wi-Fi network");
    lv_subject_set_pointer(&api->subjects.wifi_ip, "192.168.1.1");
    lv_subject_set_int(&api->subjects.door, 0); /* tell the UI the door is closed */
    lv_subject_set_int(&api->subjects.lightbulb_matter, 0); /* 0 or 1 */
    lv_subject_set_int(&api->subjects.lightbulb_zigbee, 1); /* 0 or 1 */
    lv_subject_set_int(&api->subjects.fan_matter, 0); /* 0-3 */
    lv_subject_set_int(&api->subjects.fan_zigbee, 0); /* 0 or 1*/
    lv_subject_set_int(&api->subjects.air_purifier, 3); /* 0-3 */

    lv_subject_add_observer(&api->subjects.music_play, output_subject_observer_cb, (void *)"music_play");
    lv_subject_add_observer(&api->subjects.locked, output_subject_observer_cb, (void *)"locked");
    lv_subject_add_observer(&api->subjects.volume, output_subject_observer_cb, (void *)"volume");
    lv_subject_add_observer(&api->subjects.main_light_temperature, output_subject_observer_cb,
                            (void *)"main_light_temperature");
    lv_subject_add_observer(&api->subjects.main_light_intensity, output_subject_observer_cb,
                            (void *)"main_light_intensity");
    lv_subject_add_observer(&api->subjects.thermostat_fan_speed, output_subject_observer_cb,
                            (void *)"thermostat_fan_speed");
    lv_subject_add_observer(&api->subjects.thermostat_target_temperature, output_subject_observer_cb,
                            (void *)"thermostat_target_temperature");
    lv_subject_add_observer(&api->subjects.door, output_subject_observer_cb, (void *)"door");
    lv_subject_add_observer(&api->subjects.lightbulb_matter, output_subject_observer_cb, (void *)"Matter lightbulb");
    lv_subject_add_observer(&api->subjects.lightbulb_zigbee, output_subject_observer_cb, (void *)"Zigbee lightbulb");
    lv_subject_add_observer(&api->subjects.fan_matter, output_subject_observer_cb, (void *)"Matter fan");
    lv_subject_add_observer(&api->subjects.fan_zigbee, output_subject_observer_cb, (void *)"Zigbee fan");
    lv_subject_add_observer(&api->subjects.air_purifier, output_subject_observer_cb, (void *)"air purifier");

    if (pthread_mutex_init(&delay_lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return;
    }
    if (pthread_mutex_init(&playing_now_lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return; 
    }
    pthread_create(&audio_thread, NULL, audio_play, NULL);
    pthread_create(&mqtt_sub_thread, NULL, mqtt_sub_init, (void*)api);
    pthread_create(&clock_thread, NULL, clock_init, (void*)api);
    pthread_create(&led_thread, NULL, led_blink, NULL);
    pthread_create(&button_thread, NULL, button_init, (void*)api);
    pthread_create(&adc_thread, NULL, adc_init, api);
    
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void exit_cb(int sig)
{
	if (api)
		lv_obj_delete(api->base_obj);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    lv_refr_now(NULL);
    system("cat /dev/zero > /dev/fb0 2>/dev/null");
    exit(0);
}

static void output_subject_observer_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    const char * subject_name = lv_observer_get_user_data(observer);
    LV_LOG_USER("%s output subject value: %"PRId32, subject_name, lv_subject_get_int(subject));
    if(strcmp(subject_name, "volume") == 0){
        char command[50];
        int error=0;
        sprintf(command, "amixer set PCM %d\%\t", lv_subject_get_int(subject));
        printf("%s\n", command);
        error = system(command);
    }
    else if(strcmp(subject_name, "main_light_temperature") == 0){
        int main_light_temp = lv_subject_get_int(subject);
        float fraction_delay = main_light_temp/20000.0;
        pthread_mutex_lock(&delay_lock);
        delay_millis = (int)(20.0+(1-fraction_delay)*230.0);
        pthread_mutex_unlock(&delay_lock);
    }
    else if(button_configured && strcmp(subject_name, "locked") == 0){
        lv_lock();
        int lock_status = lv_subject_get_int(subject);
        lv_indev_enable(NULL, !lock_status);
        lv_unlock();
    }
}


#endif /*LV_USE_DEMO_HIGH_RES*/
