// /* Async Request Handlers HTTP Server Example

//    This example code is in the Public Domain (or CC0 licensed, at your option.)

//    Unless required by applicable law or agreed to in writing, this
//    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.
// */
// #include "http_web_server.hpp"

// #include <esp_event.h>
// #include <esp_http_server.h>
// #include <esp_log.h>
// #include <esp_system.h>
// #include <sys/param.h>

// #include "esp_eth.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/semphr.h"

// namespace http_web_server {


// #define CONFIG_EXAMPLE_MAX_ASYNC_REQUESTS 2
// #define CONFIG_EXAMPLE_WIFI_SSID_PWD_FROM_STDIN y

// /* An example that demonstrates multiple
//    long running http requests running in parallel.

//    In this example, multiple long http request can run at
//    the same time. (uri: /long)

//    While these long requests are running, the server can still
//    respond to other incoming synchronous requests. (uri: /quick)
//  */

// #define ASYNC_WORKER_TASK_PRIORITY 5
// #define ASYNC_WORKER_TASK_STACK_SIZE 16384  // 16KB

// static const char* TAG = "example";

// // Async requests are queued here while they wait to
// // be processed by the workers
// static QueueHandle_t async_req_queue;

// // Track the number of free workers at any given time
// static SemaphoreHandle_t worker_ready_count;

// // Each worker has its own thread
// static TaskHandle_t worker_handles[CONFIG_EXAMPLE_MAX_ASYNC_REQUESTS];

// typedef esp_err_t (*httpd_req_handler_t)(httpd_req_t* req);

// typedef struct {
//   httpd_req_t* req;
//   httpd_req_handler_t handler;
// } httpd_async_req_t;

// static bool is_on_async_worker_thread(void) {
//   // is our handle one of the known async handles?
//   TaskHandle_t handle = xTaskGetCurrentTaskHandle();
//   for (int i = 0; i < CONFIG_EXAMPLE_MAX_ASYNC_REQUESTS; i++) {
//     if (worker_handles[i] == handle) {
//       return true;
//     }
//   }
//   return false;
// }

// // Submit an HTTP req to the async worker queue
// static esp_err_t submit_async_req(httpd_req_t* req, httpd_req_handler_t handler) {
//   // must create a copy of the request that we own
//   httpd_req_t* copy = NULL;
//   esp_err_t err = httpd_req_async_handler_begin(req, &copy);
//   if (err != ESP_OK) {
//     return err;
//   }

//   httpd_async_req_t async_req = {
//     .req = copy,
//     .handler = handler,
//   };

//   // How should we handle resource exhaustion?
//   // In this example, we immediately respond with an
//   // http error if no workers are available.
//   int ticks = 0;

//   // counting semaphore: if success, we know 1 or
//   // more asyncReqTaskWorkers are available.
//   if (xSemaphoreTake(worker_ready_count, ticks) == false) {
//     ESP_LOGE(TAG, "No workers are available");
//     httpd_req_async_handler_complete(copy);  // cleanup
//     return ESP_FAIL;
//   }

//   // Since worker_ready_count > 0 the queue should already have space.
//   // But lets wait up to 100ms just to be safe.
//   if (xQueueSend(async_req_queue, &async_req, pdMS_TO_TICKS(100)) == false) {
//     ESP_LOGE(TAG, "worker queue is full");
//     httpd_req_async_handler_complete(copy);  // cleanup
//     return ESP_FAIL;
//   }

//   return ESP_OK;
// }

// /* A long running HTTP GET handler */
// static esp_err_t long_async_handler(httpd_req_t* req) {
//   ESP_LOGI(TAG, "uri: /long");
//   // This handler is first invoked on the httpd thread.
//   // In order to free the httpd thread to handle other requests,
//   // we must resubmit our request to be handled on an async worker thread.
//   if (is_on_async_worker_thread() == false) {
//     // submit
//     if (submit_async_req(req, long_async_handler) == ESP_OK) {
//       return ESP_OK;
//     } else {
//       httpd_resp_set_status(req, "503 Busy");
//       httpd_resp_sendstr(req, "<div> no workers available. server busy.</div>");
//       return ESP_OK;
//     }
//   }

//   // track the number of long requests
//   static uint8_t req_count = 0;
//   req_count++;

//   // send a request count
//   char s[100];
//   snprintf(s, sizeof(s), "<div>req: %u</div>\n", req_count);
//   httpd_resp_sendstr_chunk(req, s);

//   // then every second, send a "tick"
//   for (int i = 0; i < 60; i++) {
//     // This delay makes this a "long running task".
//     // In a real application, this may be a long calculation,
//     // or some IO dependent code for instance.
//     vTaskDelay(pdMS_TO_TICKS(1000));

//     // send a tick
//     snprintf(s, sizeof(s), "<div>%u</div>\n", i);
//     auto err = httpd_resp_sendstr_chunk(req, s);
//     if (err != ESP_OK) {
//       ESP_LOGE(TAG, "Failed to send tick: %d", err);
//       return err;
//     }
//   }

//   // send "complete"
//   httpd_resp_sendstr_chunk(req, NULL);

//   return ESP_OK;
// }

// static void async_req_worker_task(void* p) {
//   ESP_LOGI(TAG, "starting async req task worker");

//   while (true) {
//     // counting semaphore - this signals that a worker
//     // is ready to accept work
//     xSemaphoreGive(worker_ready_count);

//     // wait for a request
//     httpd_async_req_t async_req;
//     if (xQueueReceive(async_req_queue, &async_req, portMAX_DELAY)) {
//       ESP_LOGI(TAG, "invoking %s", async_req.req->uri);

//       // call the handler
//       async_req.handler(async_req.req);

//       // Inform the server that it can purge the socket used for
//       // this request, if needed.
//       if (httpd_req_async_handler_complete(async_req.req) != ESP_OK) {
//         ESP_LOGE(TAG, "failed to complete async req");
//       }
//     }
//   }

//   ESP_LOGW(TAG, "worker stopped");
//   vTaskDelete(NULL);
// }

// static void start_async_req_workers(void) {
//   // counting semaphore keeps track of available workers
//   worker_ready_count = xSemaphoreCreateCounting(
//     CONFIG_EXAMPLE_MAX_ASYNC_REQUESTS,  // Max Count
//     0);                                 // Initial Count
//   if (worker_ready_count == NULL) {
//     ESP_LOGE(TAG, "Failed to create workers counting Semaphore");
//     return;
//   }

//   // create queue
//   async_req_queue = xQueueCreate(1, sizeof(httpd_async_req_t));
//   if (async_req_queue == NULL) {
//     ESP_LOGE(TAG, "Failed to create async_req_queue");
//     vSemaphoreDelete(worker_ready_count);
//     return;
//   }

//   // start worker tasks
//   for (int i = 0; i < CONFIG_EXAMPLE_MAX_ASYNC_REQUESTS; i++) {
//     bool success = xTaskCreate(
//       async_req_worker_task,
//       "async_req_worker",
//       ASYNC_WORKER_TASK_STACK_SIZE,  // stack size
//       (void*)0,                      // argument
//       ASYNC_WORKER_TASK_PRIORITY,    // priority
//       &worker_handles[i]);

//     if (!success) {
//       ESP_LOGE(TAG, "Failed to start asyncReqWorker");
//       continue;
//     }
//   }
// }

// /* A quick HTTP GET handler, which does not
//    use any asynchronous features */
// static esp_err_t quick_handler(httpd_req_t* req) {
//   ESP_LOGI(TAG, "uri: /quick");
//   char s[100];
//   snprintf(s, sizeof(s), "random: %u\n", rand());
//   httpd_resp_sendstr(req, s);
//   return ESP_OK;
// }

// static esp_err_t index_handler(httpd_req_t* req) {
//   ESP_LOGI(TAG, "uri: /");
//   const char* html =
//     "<div><a href=\"/long\">long</a></div>"
//     "<div><a href=\"/quick\">quick</a></div>"
//     "<div><a href=\"/picture\">picture</a></div>";
//   httpd_resp_sendstr(req, html);
//   return ESP_OK;
// }

// static const char* format_to_string(pixformat_t format) {
//   switch (format) {
//     case PIXFORMAT_RGB565:
//       return "RGB565";
//     case PIXFORMAT_YUV422:
//       return "YUV422";
//     case PIXFORMAT_YUV420:
//       return "YUV420";
//     case PIXFORMAT_GRAYSCALE:
//       return "GRAYSCALE";
//     case PIXFORMAT_JPEG:
//       return "JPEG";
//     case PIXFORMAT_RGB888:
//       return "RGB888";
//     case PIXFORMAT_RAW:
//       return "RAW";
//     case PIXFORMAT_RGB444:
//       return "RGB444";
//     case PIXFORMAT_RGB555:
//       return "RGB555";
//     default:
//       return "UNKNOWN";
//   }
// }

// // static esp_err_t picture_handler(httpd_req_t* req) {
// //   ESP_LOGI(TAG, "uri: /picture");
// //   camera_fb_t* pic = camera::take_picture();
// //   if (pic == NULL) {
// //     ESP_LOGE(TAG, "Camera capture failed");
// //     httpd_resp_send_500(req);
// //     return ESP_FAIL;
// //   }

// //   // Set JPEG content type header
// //   esp_err_t res = httpd_resp_set_type(req, "image/jpeg");
// //   if (res != ESP_OK) {
// //     ESP_LOGE(TAG, "Failed to set content type");
// //     camera::release_picture(pic);
// //     return res;
// //   }

// //   res = httpd_resp_send(req, (const char*)pic->buf, pic->len);
// //   if (res != ESP_OK) {
// //     ESP_LOGE(TAG, "Failed to send response");
// //   } else {
// //     ESP_LOGI(
// //       TAG,
// //       "Picture taken! Its size was: %zu bytes, width: %d, height: %d, format: %s",
// //       pic->len,
// //       pic->width,
// //       pic->height,
// //       format_to_string(pic->format));
// //   }

// //   camera::release_picture(pic);
// //   return ESP_OK;
// // }

// static esp_err_t picture_handler(httpd_req_t* req) {
//   ESP_LOGI(TAG, "uri: /picture");

//   auto jpeg_buffer = camera::copy_jpeg_buffer();
//   if (jpeg_buffer.buffer == nullptr) {
//     ESP_LOGE(TAG, "No image buffer to send");
//     httpd_resp_send_500(req);
//     return ESP_FAIL;
//   }

//   // Set the content type to JPEG
//   esp_err_t res = httpd_resp_set_type(req, "image/jpeg");
//   if (res != ESP_OK) {
//     ESP_LOGE(TAG, "Failed to set content type");
//     return res;
//   }

//   // Send the image
//   res = httpd_resp_send(req, (const char*)jpeg_buffer.buffer, jpeg_buffer.len);

//   if (res != ESP_OK) {
//     ESP_LOGE(TAG, "Failed to send response");
//   } else {
//     ESP_LOGI(TAG, "Picture served, size %zu bytes", jpeg_buffer.len);
//   }

//   return res;
// }

// static httpd_handle_t start_webserver(void) {
//   httpd_handle_t server = NULL;
//   httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//   // config.lru_purge_enable = true;

//   // If you want to force a non-default idle timeout or set other keep-alive behaviors:
//   config.keep_alive_enable = true;
//   config.keep_alive_idle = 5;      // seconds before sending TCP keep-alives
//   config.keep_alive_interval = 5;  // keep-alive packet interval
//   config.keep_alive_count = 3;     // keep-alive probe count before closing

//   // It is advisable that httpd_config_t->max_open_sockets > MAX_ASYNC_REQUESTS
//   // Why? This leaves at least one socket still available to handle
//   // quick synchronous requests. Otherwise, all the sockets will
//   // get taken by the long async handlers, and your server will no
//   // longer be responsive.
//   config.max_open_sockets = CONFIG_EXAMPLE_MAX_ASYNC_REQUESTS + 1;

//   // Start the httpd server
//   ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
//   if (httpd_start(&server, &config) != ESP_OK) {
//     ESP_LOGI(TAG, "Error starting server!");
//     return NULL;
//   }

//   const httpd_uri_t index_uri = {
//     .uri = "/",
//     .method = HTTP_GET,
//     .handler = index_handler,
//   };

//   const httpd_uri_t long_uri = {
//     .uri = "/long",
//     .method = HTTP_GET,
//     .handler = long_async_handler,
//   };

//   const httpd_uri_t quick_uri = {
//     .uri = "/quick",
//     .method = HTTP_GET,
//     .handler = quick_handler,
//   };

//   const httpd_uri_t picture_uri = {
//     .uri = "/picture",
//     .method = HTTP_GET,
//     .handler = picture_handler,
//   };

//   // Set URI handlers
//   ESP_LOGI(TAG, "Registering URI handlers");
//   httpd_register_uri_handler(server, &index_uri);
//   httpd_register_uri_handler(server, &long_uri);
//   httpd_register_uri_handler(server, &quick_uri);
//   httpd_register_uri_handler(server, &picture_uri);
//   return server;
// }

// static esp_err_t stop_webserver(httpd_handle_t server) {
//   // Stop the httpd server
//   return httpd_stop(server);
// }

// static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
//   httpd_handle_t* server = (httpd_handle_t*)arg;
//   if (*server) {
//     ESP_LOGI(TAG, "Stopping webserver");
//     if (stop_webserver(*server) == ESP_OK) {
//       *server = NULL;
//     } else {
//       ESP_LOGE(TAG, "Failed to stop http server");
//     }
//   }
// }

// static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
//   httpd_handle_t* server = (httpd_handle_t*)arg;
//   if (*server == NULL) {
//     ESP_LOGI(TAG, "Starting webserver");
//     *server = start_webserver();
//   }
// }

// httpd_handle_t setup_webserver() {
//   static httpd_handle_t server = NULL;
//   if (server != NULL) {
//     return server;
//   }

//   /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
//    * and re-start it upon connection.
//    */
//   // #ifdef CONFIG_EXAMPLE_CONNECT_WIFI
//   //   ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
//   //   ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler,
//   //   &server));
//   // #endif  // CONFIG_EXAMPLE_CONNECT_WIFI
//   // #ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
//   //   ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
//   //   ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler,
//   //   &server));
//   // #endif  // CONFIG_EXAMPLE_CONNECT_ETHERNET

//   // start workers
//   start_async_req_workers();

//   /* Start the server for the first time */
//   server = start_webserver();

//   return server;
// }
// }  // namespace http_web_server