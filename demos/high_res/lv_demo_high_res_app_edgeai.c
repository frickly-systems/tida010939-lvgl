/**
 * @file lv_demo_high_res_app_edgeai.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_demo_high_res_private.h"

#if LV_USE_DEMO_HIGH_RES && LV_BUILD_EXAMPLES && LV_USE_LABEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lvgl/lvgl.h"
#include "../../examples/lv_examples.h"
#include "../../src/widgets/image/lv_image.h"
#include "../../src/widgets/label/lv_label.h"

/*********************
 *      DEFINES
 *********************/

lv_obj_t *label;
lv_obj_t *btn;
pthread_t gst_thread;
volatile int running = 0;
const char* fifo_path = "/tmp/gst_output_fifo";

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void back_clicked_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void update_label_text(const char *text) {
    lv_label_set_text(label, text);
}

void* gst_launch_thread(void *arg) {
    char buffer[128];

    mkfifo(fifo_path, 0666);

    // Construct the GStreamer command
    char gst_command[1024];
    snprintf(gst_command, sizeof(gst_command),
             "gst-launch-1.0 alsasrc ! audioconvert ! audio/x-raw,format=S16LE,channels=1,rate=16000,layout=interleaved ! "
             "tensor_converter frames-per-tensor=3900 ! "
             "tensor_aggregator frames-in=3900 frames-out=15600 frames-flush=3900 frames-dim=1 ! "
             "tensor_transform mode=arithmetic option=typecast:float32,add:0.5,div:32767.5 ! "
             "tensor_transform mode=transpose option=1:0:2:3 ! "
             "queue leaky=2 max-size-buffers=10 ! "
             "tensor_filter framework=tensorflow2-lite model=/usr/share/oob-demo-assets/models/yamnet_audio_classification.tflite custom=Delegate:XNNPACK,NumThreads:2 ! "
             "tensor_decoder mode=image_labeling option1=/usr/share/oob-demo-assets/labels/yamnet_label_list.txt ! "
             "filesink buffer-mode=2 location=%s 1> /dev/null", fifo_path);

    FILE *pipe = popen(gst_command, "r");

    if (!pipe) {
        perror("popen failed!");
        return NULL;
    }

    int fifo_fd = open(fifo_path, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open fifo failed!");
        pclose(pipe);
        return NULL;
    }

    // Read from the named pipe
    while (running && fgets(buffer, sizeof(buffer), fdopen(fifo_fd, "r")) != NULL) {
        buffer[strcspn(buffer, "$")] = 0;
        update_label_text(buffer);
    }

    close(fifo_fd);
    pclose(pipe);
    unlink(fifo_path); // Remove the named pipe
    return NULL;
}

 // Define the callback function for the button click event
static void btn_click_event(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        if (!running) {
            running = 1;
            lv_label_set_text(lv_obj_get_child(btn, 0), "Stop");
            update_label_text("Audio classification is starting...");
            pthread_create(&gst_thread, NULL, gst_launch_thread, NULL);
        } else {
            running = 0;
            pthread_join(gst_thread, NULL);
            lv_label_set_text(lv_obj_get_child(btn, 0), "Play");
            update_label_text("Connect your microphone and press Play");
        }
    }
}

void lv_demo_high_res_app_edgeai(lv_obj_t * base_obj)
{
    lv_demo_high_res_ctx_t * c = lv_obj_get_user_data(base_obj);

    /* background */

    lv_obj_t * bg = base_obj;
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));

    lv_obj_t * bg_img = lv_image_create(bg);
    lv_subject_add_observer_obj(&c->th, lv_demo_high_res_theme_observer_image_src_cb, bg_img,
                                (void *)&c->imgs[IMG_LIGHT_BG_ABOUT]);

    lv_obj_t * bg_cont = lv_obj_create(bg);
    lv_obj_remove_style_all(bg_cont);
    lv_obj_set_size(bg_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_top(bg_cont, c->sz->gap[7], 0);
    lv_obj_set_style_pad_bottom(bg_cont, c->sz->gap[10], 0);
    lv_obj_set_style_pad_hor(bg_cont, c->sz->gap[10], 0);

    /* top margin */

    lv_obj_t * top_margin = lv_demo_high_res_top_margin_create(bg_cont, 0, true, c);

    /* app info */

    lv_obj_t * app_info = lv_demo_high_res_simple_container_create(bg_cont, true, c->sz->gap[4], LV_FLEX_ALIGN_START);
    lv_obj_align_to(app_info, top_margin, LV_ALIGN_OUT_BOTTOM_LEFT, 0, c->sz->gap[7]);

    lv_obj_t * back = lv_demo_high_res_simple_container_create(app_info, false, c->sz->gap[2], LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(back, back_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * back_icon = lv_image_create(back);
    lv_image_set_src(back_icon, c->imgs[IMG_ARROW_LEFT]);
    lv_obj_add_style(back_icon, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_A8_IMG], 0);
    lv_obj_add_flag(back_icon, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t * back_label = lv_label_create(back);
    lv_label_set_text_static(back_label, "Back");
    lv_obj_set_style_text_opa(back_label, LV_OPA_60, 0);
    lv_obj_add_style(back_label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
    lv_obj_add_style(back_label, &c->fonts[FONT_HEADING_MD], 0);

    lv_obj_t * app_label = lv_label_create(app_info);
    lv_label_set_text_static(app_label, "EdgeAI");
    lv_obj_add_style(app_label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
    lv_obj_add_style(app_label, &c->fonts[FONT_HEADING_LG], 0);

    /* Create a label */
    label = lv_label_create(bg_cont);
    lv_label_set_text(label, "Connect your microphone and press Play");
    lv_obj_set_style_text_opa(label, LV_OPA_60, 0);
    lv_obj_add_style(label, &c->styles[STYLE_COLOR_BASE][STYLE_TYPE_TEXT], 0);
    lv_obj_add_style(label, &c->fonts[FONT_HEADING_MD], 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);

    /* Create a button */
    btn = lv_btn_create(bg_cont);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 100);
    lv_obj_add_event_cb(btn, btn_click_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Play");

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 50); // Make the button circular

    lv_style_set_bg_color(&style_btn, lv_color_hex(0x00A2E8)); // Set background color
    lv_style_set_bg_grad_color(&style_btn, lv_color_hex(0x005F99)); // Set gradient color
    lv_style_set_bg_grad_dir(&style_btn, LV_GRAD_DIR_VER); // Set gradient direction

    lv_style_set_border_color(&style_btn, lv_color_hex(0xFFFFFF)); // Set border color
    lv_style_set_border_width(&style_btn, 2); // Set border width

    lv_style_set_shadow_width(&style_btn, 10); // Set shadow width
    lv_style_set_shadow_color(&style_btn, lv_color_hex(0x000000)); // Set shadow color
    lv_style_set_shadow_ofs_x(&style_btn, 5); // Set shadow offset x
    lv_style_set_shadow_ofs_y(&style_btn, 5);

    lv_obj_add_style(btn, &style_btn, 0);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void back_clicked_cb(lv_event_t * e)
{
    lv_obj_t * back = lv_event_get_target_obj(e);

    lv_obj_t * base_obj = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(back)));

    lv_obj_clean(base_obj);
    lv_demo_high_res_home(base_obj);
}

#endif
