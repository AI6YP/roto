#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"

// WIFI
#include "esp_wifi.h"
#include "esp_tls_crypto.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "secrets.h"
#include "sdkconfig.h"

#include "pages.h"

// pins
#define RAZIM         0   // pin6   -> ADC Azimuth
#define GPIO1         1   // pin7
#define RELEV         2   // pin8   -> ADC Elevation
#define GPIO3         3   // pin9
#define GPIO4         4   // pin10
#define GPIO5         5   // pin11
#define GPIO6         6   // pin12
#define GPIO7         7   // pin13
#define GPIO8         8   // pin14
#define GPIO9         9   // pin15
#define CW            14  // pin18  CW
#define GPIO15        15  // pin19
#define GPIO16        16  // pin21
#define GPIO17        17  // pin22
#define CCW           18  // pin23  CCW
#define GPIO19        19  // pin24
#define UP            20  // pin25  UP
#define GPIO21        21  // pin26
#define DOWN          22  // pin27  DOWN
#define GPIO23        23  // pin28

// ADC

typedef struct {
  int32_t val_azim;
  int32_t val_elev;
  int32_t raw_azim;
  int32_t raw_elev;
  int32_t tgt_azim;
  int32_t tgt_elev;
} state_t;

static state_t state;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(
  void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data
) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
     esp_wifi_connect();
  } else
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 100 /* EXAMPLE_ESP_MAXIMUM_RETRY */) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG,"connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_sta() {
  s_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id
  ));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip
  ));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = CONFIG_ESP_WIFI_SSID,
      .password = CONFIG_ESP_WIFI_PASSWORD,
       /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
        * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
        * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
        * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
        */
      .threshold.authmode = WIFI_AUTH_WPA2_PSK, // ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
      // .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
    },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY
  );

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened. */

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

esp_err_t index_get_handler(httpd_req_t *req) {
  /* Send a simple response */
  ESP_LOGI(TAG, "INDEX GET HANDLER");
  httpd_resp_send(req, &PAGE_index, PAGE_index_length);
  return ESP_OK;
}

esp_err_t msg_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }
  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  uint8_t *tx_buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_BINARY;
  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }
  // ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
  if (ws_pkt.len) {
    /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
    buf = calloc(ws_pkt.len, 1);
    tx_buf = calloc(8, 1);
    if (buf == NULL) {
      // ESP_LOGE(TAG, "Failed to calloc memory for buf");
      ESP_LOGE(TAG, "Failed to malloc memory for buf");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      free(buf);
      return ret;
    }
    // rdwr_i2c(i2c_master, buf, tx_buf, ws_pkt.len);

    ws_pkt.payload = (uint8_t *)&state;
    ws_pkt.len = sizeof(state);
  }

  ret = httpd_ws_send_frame(req, &ws_pkt);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
  }

  free(buf);
  return ret;
}

/* Our URI handler function to be called during POST /uri request */

httpd_uri_t uri_get = {
  .uri      = "/",
  .method   = HTTP_GET,
  .handler  = index_get_handler,
  .user_ctx = NULL
};

httpd_uri_t msg_get = {
  .uri      = "/dev1",
  .method   = HTTP_GET,
  .handler  = msg_handler,
  .user_ctx = NULL,
  .is_websocket = true
};

httpd_uri_t msg_post = {
  .uri      = "/dev1",
  .method   = HTTP_POST,
  .handler  = msg_handler,
  .user_ctx = NULL,
  .is_websocket = true
};


httpd_handle_t start_webserver() {
  /* Generate default configuration */
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 16384;

  /* Empty handle to esp_http_server */
  httpd_handle_t server = NULL;

  /* Start the httpd server */
  if (httpd_start(&server, &config) == ESP_OK) {
      /* Register URI handlers */
      httpd_register_uri_handler(server, &uri_get);
      httpd_register_uri_handler(server, &msg_get);
      httpd_register_uri_handler(server, &msg_post);
  }
  /* If server failed to start, handle will be NULL */
  return server;
}

void app_main(void) {

  // GPIO
  gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = 0,
    .pull_down_en = 0
  };

  io_conf.mode = GPIO_MODE_INPUT,
  io_conf.pin_bit_mask = (1ULL << RAZIM);
  gpio_config(&io_conf);
  io_conf.pin_bit_mask = (1ULL << RELEV);
  gpio_config(&io_conf);

  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << CW);
  gpio_config(&io_conf);
  io_conf.pin_bit_mask = (1ULL << CCW);
  gpio_config(&io_conf);
  io_conf.pin_bit_mask = (1ULL << UP);
  gpio_config(&io_conf);
  io_conf.pin_bit_mask = (1ULL << DOWN);
  gpio_config(&io_conf);

  // ADC
  adc_oneshot_unit_handle_t adc_handle;

  adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

  adc_oneshot_chan_cfg_t azim_chan_config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12, // 0, 2_5, 6, 12
  };

  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_0, &azim_chan_config));

  adc_oneshot_chan_cfg_t elev_chan_config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12, // 0, 2_5, 6, 12
  };

  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_2, &elev_chan_config));

  {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
  }

  ESP_LOGI(TAG, "HELLO ROTO");

  wifi_init_sta();

  httpd_handle_t server = NULL;
  server = start_webserver();

  state.tgt_azim = 122000;
  state.tgt_elev = 22000;

  while(1) {
    vTaskDelay(100 / portTICK_PERIOD_MS); // 1000ms polling
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &state.raw_azim));
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_2, &state.raw_elev));
    state.val_azim = state.raw_azim * 125 -210000;
    state.val_elev = state.raw_elev * 33   -10000;

    int dif_azim = state.val_azim - state.tgt_azim;
    if (abs(dif_azim) < 4000) {
        gpio_set_level(CW, 0); gpio_set_level(CCW, 0);
    } else {
      if (dif_azim < 0) {
        gpio_set_level(CW, 1); gpio_set_level(CCW, 0);
      } else {
        gpio_set_level(CW, 0); gpio_set_level(CCW, 1);
      }
    }
    int dif_elev = state.val_elev - state.tgt_elev;
    if (abs(dif_elev) < 4000) {
      gpio_set_level(UP, 0); gpio_set_level(DOWN, 0);
    } else {
      if (dif_elev < 0) {
        gpio_set_level(UP, 1); gpio_set_level(DOWN, 0);
      } else {
        gpio_set_level(UP, 0); gpio_set_level(DOWN, 1);
      }
    }
  }
}
