/*
 * This example shows how to write a client that subscribes to a topic and does
 * not do anything other than handle the messages that are received.
 */

#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lv_demo_high_res_private.h"
#include "../../src/osal/lv_os.h"

static lv_demo_high_res_api_t * api_hmi;

/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect_vol_sub(struct mosquitto *mosq, void *obj, int reason_code)
{
  int rc;
  /* Print out the connection result. mosquitto_connack_string() produces an
   * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
   * clients is mosquitto_reason_string().
   */
  printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
  if(reason_code != 0){
        /* If the connection fails for any reason, we don't want to keep on
         * retrying in this example, so disconnect. Without this, the client
         * will attempt to reconnect. */
        mosquitto_disconnect(mosq);
  }

  /* Making subscriptions in the on_connect() callback means that if the
   * connection drops and is automatically resumed by the client, then the
   * subscriptions will be recreated when the client reconnects. */
  rc = mosquitto_subscribe(mosq, NULL, "SmartHome/volume", 1);
  if(rc != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
        /* We might as well disconnect if we were unable to subscribe */
        mosquitto_disconnect(mosq);
  }
  rc = mosquitto_subscribe(mosq, NULL, "SmartHome/led", 1);
  if(rc != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
        /* We might as well disconnect if we were unable to subscribe */
        mosquitto_disconnect(mosq);
  }
}


/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void on_subscribe_vol_sub(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
  int i;
  bool have_subscription = false;

  /* In this example we only subscribe to a single topic at once, but a
   * SUBSCRIBE can contain many topics at once, so this is one way to check
   * them all. */
  for(i=0; i<qos_count; i++){
        printf("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
        if(granted_qos[i] <= 2){
                have_subscription = true;
        }
  }
  if(have_subscription == false){
        /* The broker rejected all of our subscriptions, we know we only sent
         * the one SUBSCRIBE, so there is no point remaining connected. */
        fprintf(stderr, "Error: All subscriptions rejected.\n");
        mosquitto_disconnect(mosq);
  }
}


/* Callback called when the client receives a message. */
void on_message_vol_sub(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
  if(strcmp((char*)msg->topic, "SmartHome/volume")==0){  
    int vol = atoi((char *)msg->payload);
    if(vol<=0){
      vol=0;
    }
    else if(vol >=100){
      vol=100;
    }    
    lv_lock();
    lv_subject_set_int(&api_hmi->subjects.volume, vol);
    lv_unlock();
    char command[50];
    int error=0;
    sprintf(command, "amixer set PCM %d\%\t", vol);
    printf("%s\n", command);
    error = system(command);
    printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
  }
  else if(strcmp((char*)msg->topic, "SmartHome/led")==0){
    int led = atoi((char *)msg->payload);
    if(led<=0){
      led=0;
    }
    else if(led >=20000){
      led=20000;
    } 
    lv_lock();
    lv_subject_set_int(&api_hmi->subjects.main_light_temperature, led);
    lv_unlock();
  }
}


void *mqtt_sub_init(void * api){
  struct mosquitto *mosq;
  int rc;
  api_hmi = (lv_demo_high_res_api_t*)api;

  /* Required before calling other mosquitto functions */
  mosquitto_lib_init();

  /* Create a new client instance.
   * id = NULL -> ask the broker to generate a client id for us
   * clean session = true -> the broker should remove old sessions when we connect
   * obj = NULL -> we aren't passing any of our private data for callbacks
   */
  mosq = mosquitto_new(NULL, true, NULL);
  if(mosq == NULL){
        fprintf(stderr, "Error: Out of memory.\n");
        return NULL;
  }

  /* Configure callbacks. This should be done before connecting ideally. */
  mosquitto_connect_callback_set(mosq, on_connect_vol_sub);
  mosquitto_subscribe_callback_set(mosq, on_subscribe_vol_sub);
  mosquitto_message_callback_set(mosq, on_message_vol_sub);

  mosquitto_tls_set(mosq, "/usr/share/ti-lvgl-demo/cert/AmazonRootCA1.pem", NULL, NULL, NULL, NULL);

  /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
   * This call makes the socket connection only, it does not complete the MQTT
   * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */
  rc = mosquitto_connect(mosq, "broker.hivemq.com", 8883, 60);
  printf("rc=%d\t MOSQ_ERR_SUCCESS=%d\n", rc, MOSQ_ERR_SUCCESS);
  if(rc != MOSQ_ERR_SUCCESS){
        mosquitto_destroy(mosq);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return NULL;
  }

  /* Run the network loop in a blocking call. The only thing we do in this
   * example is to print incoming messages, so a blocking call here is fine.
   *
   * This call will continue forever, carrying automatic reconnections if
   * necessary, until the user calls mosquitto_disconnect().
   */
  mosquitto_loop_forever(mosq, -1, 1);

  mosquitto_lib_cleanup();
  return NULL;
}
