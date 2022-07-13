#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_err.h"

#include <string.h>
#include <sys/param.h>
#include "t800.h"
#include "protocol_examples_common.h"
#include "nvs_flash.h"

#define MENUCONFIG_PORT             CONFIG_EXAMPLE_PORT
#define KEEPALIVE_IDLE              CONFIG_EXAMPLE_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL          CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT             CONFIG_EXAMPLE_KEEPALIVE_COUNT
#define ATTACKER_ADDRESS            CONFIG_ATTACKER_ADDRESS
#define ESP32_ADDRESS               CONFIG_ESP32_ADDRESS
#define ATTACKER_PORT               CONFIG_ATTACKER_PORT
#define ATTACKER_LOG_PORT           CONFIG_ATTACKER_LOG_PORT
#define IPERF_PORT                  CONFIG_IPERF_PORT
#define SAMPLING_INTERVAL           CONFIG_SAMPLING_INTERVAL
#define BENCHMARK_DURATION          CONFIG_BENCHMARK_DURATION

static const char *TAG = "Iperf Benchmark";

typedef struct {
    int sock;
    struct sockaddr_in addr;
} exp_arg_t;

bool is_finish = false;

char send_msg(char* msg, int msg_socket, struct sockaddr_in* to_addr);
int iperf_setup_tcp_server(struct sockaddr_in *listen_addr);
void setup_experiment(exp_arg_t* arg);
static void iperf_tcp_server(int attacker_sock);
static void experiment_runner_task(void *pvParameters);
void measurer_task(void *pvParameters);

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    // 1. Configuring our UDP socket
    struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_family = AF_INET,
        .sin_port = htons(MENUCONFIG_PORT)    // MENUCONFIG_PORT
    };

    // 2. Opening the UDP socket
    int attacker_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (attacker_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }

    // 3. Binding our UDP socket so we can receive incoming packets
    if (bind(attacker_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }
    ESP_LOGI(TAG, "Socket bound to port %d (from menuconfig)", MENUCONFIG_PORT);

    struct sockaddr_in attacker_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = 0,   // we set this in inet_pton below
        .sin_port = htons(ATTACKER_PORT)
    };
    inet_pton(AF_INET, ATTACKER_ADDRESS, &(attacker_addr.sin_addr));

    exp_arg_t *arg = malloc(sizeof(exp_arg_t));
    arg->sock = attacker_sock; 
    arg->addr = attacker_addr;
    setup_experiment(arg);

#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(measurer_task, "measurer", 4096, (void *)arg, 4, NULL);
    xTaskCreate(experiment_runner_task, "experiment_runner", 4096, (void *)arg, 5, NULL);
#endif
}

char send_msg(char* msg, int msg_socket, struct sockaddr_in* to_addr) {
    char answer = '\0';
    while (answer == '\0') {
        int read_bytes = sendto(msg_socket, msg, strlen(msg), 0, (struct sockaddr*) to_addr, sizeof(struct sockaddr_in));
        ESP_LOGI(TAG, "sent %s", msg);
        recv(msg_socket, &answer, sizeof(answer), 0);
    }

    return answer;
}

int iperf_setup_tcp_server(struct sockaddr_in *listen_addr) {
    int ret = -1;
    int client_socket = -1;

    listen_addr->sin_family = AF_INET;
    listen_addr->sin_port = htons(IPERF_PORT);
    inet_pton(AF_INET, ESP32_ADDRESS, &(listen_addr->sin_addr));

    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    ESP_GOTO_ON_FALSE((listen_socket >= 0), ESP_FAIL, exit, TAG, "Unable to create socket: errno %d", errno);

    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created iperf");

    int err = 0;
    err = bind(listen_socket, (struct sockaddr *)listen_addr, sizeof(struct sockaddr_in));
    ESP_GOTO_ON_FALSE((err == 0), ESP_FAIL, exit, TAG, "Socket unable to bind: errno %d, IPPROTO: %d", errno, AF_INET);
    
    err = listen(listen_socket, 5);
    ESP_GOTO_ON_FALSE((err == 0), ESP_FAIL, exit, TAG, "Error occurred during listen: errno %d", errno);

    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);
    client_socket = accept(listen_socket, (struct sockaddr *)&remote_addr, &len);
    ESP_LOGW(TAG, "CLIENT_SOCKET: %d", client_socket);
    if (client_socket == -1) {
        perror("erro no accept");
    }
    ESP_GOTO_ON_FALSE((client_socket >= 0), ESP_FAIL, exit, TAG, "Unable to accept connection: errno %d", errno);
    ESP_LOGE(TAG, "accept - ip: %s port: %d\n", inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port));

    struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 };
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

exit:
    return client_socket;
}

static void iperf_tcp_server(int attacker_sock) {
    struct sockaddr_in listen_addr = { 0 };
    int recv_socket = iperf_setup_tcp_server(&listen_addr); 
    socklen_t socklen = sizeof(listen_addr);
    uint8_t *buffer = malloc(16 << 10);
    int want_recv = 16 << 10;
    
    is_finish = false;
    while (!is_finish) {
        int result = recvfrom(recv_socket, buffer, want_recv, 0, (struct sockaddr*)&listen_addr, &socklen);
        if (result < 0) {
            ESP_LOGE(TAG, "errno recv: %d", errno);
            perror("erro");
        } else {
            /* exp_bandwidth += result; */
        }
    }
    
    ESP_LOGW(TAG, "finished running iperf");

    // Wait for attacker to signal that experiment is over
    int32_t len;
    char rx_buffer[3];
    do {
        len = recv(attacker_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "recv() error");
            perror(NULL);
        } else if (len == 0) {
            ESP_LOGE(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
        }
    } while (rx_buffer[0] != 'D');
}

void setup_experiment(exp_arg_t* arg) {
    // 1. Signal experiment start to attacker by sending "start"
    // 2. Attacker responds with experiment's tree (ascii byte in ["6", "7", "8", "9", "0", "1", "2", "r"])
    char chosen_tree = send_msg("start", arg->sock, &arg->addr);
    ESP_LOGI(TAG, "chosen_tree: %c", chosen_tree);

    // 3. Assign t800 function pointer to tree chosen by attacker
    t800_config_t config = {
        .eval = NULL,
        .mode = SELECTED
    };
    switch (chosen_tree) {
        case '0':
            config.eval = decision_tree_depth_10;
            break;
        case '2':
            config.eval = decision_tree_depth_12;
            break;
        case 'm':
            config.eval = mlp;
            break;
        case 'n':
            config.mode = UNINITIALIZED;
            break;
        default:
            ESP_LOGE(TAG, "Invalid tree selected by attacker: %c", chosen_tree);
            return;
    }

    t800_init(config);

    // 4. Signal that ESP32 assigned the tree previously sent and experiment is ready to begin
    send_msg("assigned", arg->sock, &arg->addr);
}

static void experiment_runner_task(void *pvParameters) {
    exp_arg_t *arg = (exp_arg_t *)pvParameters;
    iperf_tcp_server(arg->sock);

    // Signal to attacker that esp will restart
    send_msg("complete", arg->sock, &arg->addr);

    // Close all sockets and reboot
    ESP_LOGI(TAG, "Experiment over, rebooting...");
    esp_restart();
}

void measurer_task(void *pvParameters) {
    exp_arg_t arg = *(exp_arg_t*)(pvParameters);

    // JSON size: 500 base + 100 per task
    size_t stats_len = (uxTaskGetNumberOfTasks() * 100) + 500;
    char* runtime_stats = malloc(stats_len);
    if (!runtime_stats) {
        ESP_LOGE(TAG, "malloc error on runtime_stats");
        return;
    }

    // Get handle to "wifi" task (which houses the lwip stack)
    TaskHandle_t wifi_task = xTaskGetHandle("wifi");
    if (!wifi_task) {
        ESP_LOGE(TAG, "wifi task not found");
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = 0,   // we set this in inet_pton below
        .sin_port = htons(ATTACKER_LOG_PORT)
    };
    inet_pton(AF_INET, ATTACKER_ADDRESS, &(addr.sin_addr));

    uint32_t cur = 0;
    while (cur < BENCHMARK_DURATION) {
        // Gather freertos tasks data
        ESP_LOGI(TAG, "Sending stats...");
        
        vTaskGetRunTimeStats(runtime_stats);
        int size_stats = strlen(runtime_stats);
        runtime_stats[size_stats] = '\0';
        
        ESP_LOGW(TAG, "Timestamp: %u", cur);
        sprintf(&runtime_stats[size_stats], "Timestamp\t%u", cur);
        /* size_stats += strlen(runtime_stats+size_stats); */
        size_stats = strlen(runtime_stats);
        /* runtime_stats[size_stats] = '\0'; */

        // Generate and append Stack stats to collected data
        UBaseType_t stack_stats = uxTaskGetStackHighWaterMark(wifi_task);
        sprintf(&runtime_stats[size_stats], "\r\nStack\t%u", stack_stats);

        // Get network bandwidth stats
        /* size_stats += strlen(runtime_stats+size_stats); */
        size_stats = strlen(runtime_stats);
        /* runtime_stats[size_stats] = '\0'; */

        double actual_bandwidth = (exp_bandwidth / 1e6 * 8) / SAMPLING_INTERVAL;
        sprintf(&runtime_stats[size_stats], "\r\nMbps\t%.2f", actual_bandwidth);
        printf("actual_bandwidth: %.2f |-------------\n", actual_bandwidth);

        cur += SAMPLING_INTERVAL;
               
        // Reset network measuring after experiment.
        exp_bandwidth = 0;
        ESP_LOGE(TAG, "packet_counter: %u", packet_counter);

        // Sending experiment data...
        int sent_bytes = sendto(arg.sock, runtime_stats, strlen(runtime_stats), 0, (struct sockaddr*) &addr, sizeof(addr));
        /*ESP_LOGI(TAG, "run_time_stats: %s", runtime_stats);*/

        // Put the thread to sleep and do the next iteration after `sleep_duration`
        const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
        vTaskDelay(xDelay);
    }
    is_finish = true;

    vTaskDelete(NULL);
}

