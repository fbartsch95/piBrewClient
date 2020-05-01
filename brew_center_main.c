#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>
#include "gattlib.h"

#define BLE_SCAN_TIMEOUT   4
#define BREW_SERVER_NAME "brewController"
#define ADVERTISED_NAME_LENGTH 29 
#define LED_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"

typedef void (*ble_discover_device_t)(const char* addr, const char* name);
typedef enum {READ, WRITE} operation_t;
// We use a mutex to make the BLE connections synchronous
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static uuid_t led_uuid;
LIST_HEAD(listhead, connection_t) g_ble_connections;

struct connection_t
{
	pthread_t thread;
	gatt_connection_t* handle;
	char* addr;
	LIST_ENTRY(connection_t) entries;
};

typedef char bleAdvertisedName[ADVERTISED_NAME_LENGTH];

static void *ble_connect_device(void *arg)
{
	struct connection_t *connection = arg;
	char* addr = connection->addr;
	pthread_mutex_lock(&g_mutex);
	printf("Trying to connect...");
	connection->handle = gattlib_connect(NULL, addr, GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT);
	if (connection->handle == NULL)
	{
		fprintf(stderr, "Fail to connect to the bluetooth device.\n");
		return NULL;
	}
	puts("Succeeded to connect to the bluetooth device.");
	pthread_mutex_unlock(&g_mutex);
	return NULL;
}

void switch_led(gatt_connection_t* connection)
{
	uint8_t i = 0;
	int ret;
	uint8_t* led_state;
	uint8_t new_state;
	uint32_t len;
	const char* led_uuid_str = LED_UUID;
	if (gattlib_string_to_uuid(led_uuid_str, strlen(led_uuid_str) + 1, &led_uuid) < 0)
	{
		printf("UUID fuckup!\n");
		return;
	}
	ret = gattlib_read_char_by_uuid(connection, &led_uuid, (void **)&led_state, &len);
	if (ret != GATTLIB_SUCCESS)
	{
		printf("Read operation failed!");
		return;
	}
	printf("Read UUID completed: ");
	for (i = 0; i < len; i++)
	{
		printf("LED state: %02x ", led_state[i]);
	}
	printf("\n");
	new_state = led_state[0] ^ 0x01 ;
	ret = gattlib_write_char_by_uuid(connection, &led_uuid, &new_state, sizeof(uint8_t));	
	if (ret != GATTLIB_SUCCESS)
	{
		printf("Write operation failed!");
		return;
	}
	free(led_state);
}

static void ble_filter_by_name(void *adapter, const char* addr, const char* name, void *user_data)
{
	struct connection_t *connection;
	int ret;
	if (!user_data)
	{
		printf("No name filter provided!");
		return;
	}
	
	bleAdvertisedName filter_name;
	memcpy(filter_name, user_data, sizeof(bleAdvertisedName));
	if (!name)
	{
		printf("Ignored %s\n", addr);
		return;
	}
	if (strcmp(name, filter_name) != 0)
	{
		printf("Ignored %s: %s \n", name, addr);
		return;
	}
	printf("Discovered %s: %s \n", filter_name, addr);

	connection = malloc(sizeof(struct connection_t));
	if (connection == NULL)
	{
		fprintf(stderr, "Failt to allocate connection.\n");
		return;
	}
	connection->addr = strdup(addr);

	ret = pthread_create(&connection->thread, NULL,	ble_connect_device, connection);
	if (ret != 0)
	{
		fprintf(stderr, "Failt to create BLE connection thread.\n");
		free(connection);
		return;
	}
	printf("Filter by name completed!");
	LIST_INSERT_HEAD(&g_ble_connections, connection, entries);
}

int main(int argc, const char *argv[])
{
	bleAdvertisedName brewServerName = BREW_SERVER_NAME;
	const char* adapter_name;
	void* adapter;
	int ret;

	if (argc == 1)
	{
		adapter_name = NULL;
	}
	else if (argc == 2)
	{
		adapter_name = argv[1];
	}
	else
	{
		fprintf(stderr, "%s [<bluetooth-adapter>]\n", argv[0]);
		return 1;
	}

	LIST_INIT(&g_ble_connections);

	ret = gattlib_adapter_open(adapter_name, &adapter);
	if (ret)
	{
		fprintf(stderr, "ERROR: Failed to open adapter.\n");
		return 1;
	}

	pthread_mutex_lock(&g_mutex);
	ret = gattlib_adapter_scan_enable(adapter, ble_filter_by_name, BLE_SCAN_TIMEOUT
									 , (void*) brewServerName /* user_data */);
	if (ret)
	{
		fprintf(stderr, "ERROR: Failed to scan.\n");
		goto EXIT;
	}

	gattlib_adapter_scan_disable(adapter);

	puts("Scan completed");
	pthread_mutex_unlock(&g_mutex);

	// Wait for the thread to complete
	while (g_ble_connections.lh_first != NULL)
	{
		struct connection_t* connection = g_ble_connections.lh_first;
		pthread_join(connection->thread, NULL);
		switch_led(connection->handle);
		gattlib_disconnect(connection->handle);
		LIST_REMOVE(g_ble_connections.lh_first, entries);
		free(connection->addr);
		free(connection);
	}

EXIT:
	gattlib_adapter_close(adapter);
	return ret;
}
