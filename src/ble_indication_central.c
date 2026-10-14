#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "gattlib.h"

#define SCAN_SECONDS 10
#define INDICATION_SECONDS 30
#define MAX_DEVICES 64
#define DEFAULT_CHARACTERISTIC "0000fff1-0000-1000-8000-00805f9b34fb"

typedef struct {
    char address[32];
    char name[128];
} ble_device_t;

static ble_device_t devices[MAX_DEVICES];
static size_t device_count;
static pthread_mutex_t devices_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t setup_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t setup_condition = PTHREAD_COND_INITIALIZER;
static int setup_finished;
static int setup_succeeded;
static gattlib_connection_t *active_connection;
static const char *target_characteristic = DEFAULT_CHARACTERISTIC;

static int same_text_case_insensitive(const char *left, const char *right) {
    while (*left && *right) {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static void on_device_found(gattlib_adapter_t *adapter, const char *address,
                            const char *name, void *user_data) {
    size_t i;
    (void)adapter;
    (void)user_data;

    pthread_mutex_lock(&devices_mutex);
    for (i = 0; i < device_count; i++) {
        if (same_text_case_insensitive(devices[i].address, address)) {
            if (name && name[0] != '\0') {
                snprintf(devices[i].name, sizeof(devices[i].name), "%s", name);
            }
            pthread_mutex_unlock(&devices_mutex);
            return;
        }
    }

    if (device_count < MAX_DEVICES) {
        snprintf(devices[device_count].address,
                 sizeof(devices[device_count].address), "%s", address);
        snprintf(devices[device_count].name, sizeof(devices[device_count].name),
                 "%s", (name && name[0] != '\0') ? name : "(name not advertised)");
        device_count++;
    }
    pthread_mutex_unlock(&devices_mutex);
}

static void on_indication(const uuid_t *uuid, const uint8_t *data,
                          size_t data_length, void *user_data) {
    size_t i;
    char uuid_text[MAX_LEN_UUID_STR + 1];
    (void)user_data;

    gattlib_uuid_to_string(uuid, uuid_text, sizeof(uuid_text));
    printf("\n<<< Indication from %s (%zu byte%s): ", uuid_text, data_length,
           data_length == 1 ? "" : "s");
    for (i = 0; i < data_length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    fflush(stdout);
}

static int choose_device(char *address_out, size_t address_size) {
    size_t i;
    int choice;

    pthread_mutex_lock(&devices_mutex);
    if (device_count == 0) {
        pthread_mutex_unlock(&devices_mutex);
        fprintf(stderr, "No BLE devices were found. Keep the phone advertising and try again.\n");
        return -1;
    }

    printf("\nDevices found:\n");
    for (i = 0; i < device_count; i++) {
        printf("  %zu) %-24s %s\n", i + 1, devices[i].name, devices[i].address);
    }
    pthread_mutex_unlock(&devices_mutex);

    printf("Choose your phone by number: ");
    fflush(stdout);
    if (scanf("%d", &choice) != 1 || choice < 1 || (size_t)choice > device_count) {
        fprintf(stderr, "That selection is not in the list.\n");
        return -1;
    }

    pthread_mutex_lock(&devices_mutex);
    snprintf(address_out, address_size, "%s", devices[choice - 1].address);
    pthread_mutex_unlock(&devices_mutex);
    return 0;
}

static void show_gatt_table(gattlib_connection_t *connection) {
    gattlib_primary_service_t *services = NULL;
    gattlib_characteristic_t *characteristics = NULL;
    int services_count = 0;
    int characteristics_count = 0;
    int ret;
    int i;
    char uuid_text[MAX_LEN_UUID_STR + 1];

    ret = gattlib_discover_primary(connection, &services, &services_count);
    if (ret != GATTLIB_SUCCESS) {
        printf("Could not list services (Gattlib error %d).\n", ret);
    } else {
        printf("\nGATT services:\n");
        for (i = 0; i < services_count; i++) {
            gattlib_uuid_to_string(&services[i].uuid, uuid_text, sizeof(uuid_text));
            printf("  handles 0x%04X-0x%04X  %s\n",
                   services[i].attr_handle_start, services[i].attr_handle_end,
                   uuid_text);
        }
        free(services);
    }

    ret = gattlib_discover_char(connection, &characteristics, &characteristics_count);
    if (ret != GATTLIB_SUCCESS) {
        printf("Could not list characteristics (Gattlib error %d).\n", ret);
        return;
    }

    printf("\nGATT characteristics:\n");
    for (i = 0; i < characteristics_count; i++) {
        gattlib_uuid_to_string(&characteristics[i].uuid, uuid_text, sizeof(uuid_text));
        printf("  value handle 0x%04X  properties 0x%02X  %s%s\n",
               characteristics[i].value_handle, characteristics[i].properties,
               uuid_text,
               (characteristics[i].properties & GATTLIB_CHARACTERISTIC_INDICATE)
                   ? "  [indicate]" : "");
    }
    free(characteristics);
}

static void on_device_connected(gattlib_adapter_t *adapter, const char *address,
                               gattlib_connection_t *connection, int error,
                               void *user_data) {
    gattlib_characteristic_t *characteristics = NULL;
    int characteristics_count = 0;
    int ret;
    int i;
    int found = 0;
    uint8_t selected_properties = 0;
    uuid_t characteristic_uuid;
    char uuid_text[MAX_LEN_UUID_STR + 1];
    (void)adapter;
    (void)user_data;

    if (error != GATTLIB_SUCCESS || connection == NULL) {
        fprintf(stderr, "Could not connect to %s (Gattlib error %d).\n", address, error);
        goto finish;
    }
    active_connection = connection;
    printf("Connected to %s.\n", address);
    show_gatt_table(connection);

    ret = gattlib_string_to_uuid(target_characteristic,
                                 strlen(target_characteristic) + 1,
                                 &characteristic_uuid);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "Invalid characteristic UUID: %s\n", target_characteristic);
        goto finish;
    }

    ret = gattlib_discover_char(connection, &characteristics, &characteristics_count);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "Could not discover characteristics (Gattlib error %d).\n", ret);
        goto finish;
    }
    for (i = 0; i < characteristics_count; i++) {
        if (gattlib_uuid_cmp(&characteristics[i].uuid, &characteristic_uuid) == 0) {
            found = 1;
            selected_properties = characteristics[i].properties;
            break;
        }
    }
    free(characteristics);
    characteristics = NULL;

    if (!found) {
        gattlib_uuid_to_string(&characteristic_uuid, uuid_text, sizeof(uuid_text));
        fprintf(stderr, "Characteristic %s was not found. Check --char and the phone's GATT server.\n",
                uuid_text);
        goto finish;
    }
    if ((selected_properties & GATTLIB_CHARACTERISTIC_INDICATE) == 0) {
        fprintf(stderr, "The selected characteristic does not advertise the Indicate property.\n");
        goto finish;
    }

    ret = gattlib_register_indication(connection, on_indication, NULL);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "Could not register indication callback (Gattlib error %d).\n", ret);
        goto finish;
    }
    ret = gattlib_indication_start(connection, &characteristic_uuid);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "Could not enable indications (Gattlib error %d).\n", ret);
        goto finish;
    }

    printf("Indications enabled (CCCD 0x0002). Send an indication from the phone now.\n");
    setup_succeeded = 1;

finish:
    if (characteristics != NULL) {
        free(characteristics);
    }
    pthread_mutex_lock(&setup_mutex);
    setup_finished = 1;
    pthread_cond_signal(&setup_condition);
    pthread_mutex_unlock(&setup_mutex);
}

static void *ble_task(void *unused) {
    gattlib_adapter_t *adapter = NULL;
    int ret;
    int connected_ok;
    char selected_address[32];
    (void)unused;

    ret = gattlib_adapter_open(NULL, &adapter);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "Could not open the Bluetooth adapter (Gattlib error %d).\n", ret);
        goto done;
    }

    printf("Scanning for %d seconds. Make sure your phone is advertising...\n", SCAN_SECONDS);
    ret = gattlib_adapter_scan_enable(adapter, on_device_found, SCAN_SECONDS, NULL);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "BLE scan failed (Gattlib error %d).\n", ret);
        goto done;
    }

    if (choose_device(selected_address, sizeof(selected_address)) != 0) {
        goto done;
    }

    printf("\nConnecting to %s...\n", selected_address);
    ret = gattlib_connect(NULL, selected_address,
                          GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT,
                          on_device_connected, NULL);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "Could not start connection (Gattlib error %d).\n", ret);
        goto done;
    }
    pthread_mutex_lock(&setup_mutex);
    while (!setup_finished) {
        pthread_cond_wait(&setup_condition, &setup_mutex);
    }
    connected_ok = setup_succeeded;
    pthread_mutex_unlock(&setup_mutex);
    if (connected_ok) {
        printf("Listening for %d seconds...\n", INDICATION_SECONDS);
        g_usleep(INDICATION_SECONDS * G_USEC_PER_SEC);
    }

done:
    if (active_connection != NULL) {
        gattlib_disconnect(active_connection, true);
    }
    if (adapter != NULL) {
        gattlib_adapter_close(adapter);
    }
    return NULL;
}

static void print_usage(const char *program) {
    printf("Usage: %s [--char CHARACTERISTIC_UUID]\n", program);
    printf("Default characteristic: %s\n", DEFAULT_CHARACTERISTIC);
}

int main(int argc, char **argv) {
    int ret;

    if (argc == 3 && strcmp(argv[1], "--char") == 0) {
        target_characteristic = argv[2];
    } else if (argc != 1) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    ret = gattlib_mainloop(ble_task, NULL);
    if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "Gattlib event loop failed (error %d).\n", ret);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
