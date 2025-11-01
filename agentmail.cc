/**
 * AgentMail REST API Client Implementation
 * 
 * This file implements a C++ wrapper around the AgentMail REST API
 * for use in ESP32 projects with ESP-IDF 5.5+
 */

#include "agentmail.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>

#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include <esp_crt_bundle.h>
#endif

static const char *TAG = "agentmail";
static const char *DEFAULT_BASE_URL = "https://api.agentmail.to/v0";
static const int DEFAULT_TIMEOUT_MS = 10000;
static const int MAX_HTTP_RESPONSE_SIZE = 32768; // 32KB

/**
 * Internal client structure
 */
typedef struct {
    char *api_key;
    char *base_url;
    int timeout_ms;
    bool enable_logging;
    void *ctx;
} agentmail_client_t;

/**
 * HTTP response buffer
 */
typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
} http_response_t;

/**
 * HTTP event handler for accumulating response data
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_response_t *response = (http_response_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Ensure buffer capacity
                size_t new_size = response->size + evt->data_len;
                if (new_size >= response->capacity) {
                    // Grow buffer (double or fit new size)
                    size_t new_capacity = (new_size * 2 > MAX_HTTP_RESPONSE_SIZE) 
                                          ? MAX_HTTP_RESPONSE_SIZE 
                                          : new_size * 2;
                    if (new_capacity <= response->capacity) {
                        ESP_LOGE(TAG, "Response too large, dropping data");
                        break;
                    }
                    char *new_buffer = (char *)realloc(response->buffer, new_capacity);
                    if (new_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to grow response buffer");
                        break;
                    }
                    response->buffer = new_buffer;
                    response->capacity = new_capacity;
                }
                // Append data
                memcpy(response->buffer + response->size, evt->data, evt->data_len);
                response->size += evt->data_len;
                response->buffer[response->size] = '\0';
            }
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP event error");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * Helper function to perform HTTP request
 */
static agentmail_err_t perform_http_request(
    agentmail_client_t *client,
    const char *method,
    const char *path,
    const char *body,
    http_response_t *response,
    int *status_code
) {
    if (client == NULL || path == NULL || response == NULL || status_code == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    // Build full URL
    char url[512];
    snprintf(url, sizeof(url), "%s%s", client->base_url, path);

    if (client->enable_logging) {
        ESP_LOGI(TAG, "%s %s", method, url);
        if (body) {
            ESP_LOGD(TAG, "Body: %s", body);
        }
    }

    // Initialize response buffer
    response->buffer = (char *)calloc(1, 4096);
    if (response->buffer == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }
    response->capacity = 4096;
    response->size = 0;

    // Configure HTTP client
    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = client->timeout_ms,
        .event_handler = http_event_handler,
        .user_data = response,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&http_config);
    if (http_client == NULL) {
        free(response->buffer);
        return AGENTMAIL_ERR_HTTP;
    }

    // Set method
    if (strcmp(method, "GET") == 0) {
        esp_http_client_set_method(http_client, HTTP_METHOD_GET);
    } else if (strcmp(method, "POST") == 0) {
        esp_http_client_set_method(http_client, HTTP_METHOD_POST);
    } else if (strcmp(method, "PUT") == 0) {
        esp_http_client_set_method(http_client, HTTP_METHOD_PUT);
    } else if (strcmp(method, "DELETE") == 0) {
        esp_http_client_set_method(http_client, HTTP_METHOD_DELETE);
    } else if (strcmp(method, "PATCH") == 0) {
        esp_http_client_set_method(http_client, HTTP_METHOD_PATCH);
    }

    // Set headers
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", client->api_key);
    esp_http_client_set_header(http_client, "Authorization", auth_header);
    esp_http_client_set_header(http_client, "Content-Type", "application/json");
    esp_http_client_set_header(http_client, "User-Agent", "PlaiPin-AgentMail/1.0");

    // Set body if provided
    if (body != NULL) {
        esp_http_client_set_post_field(http_client, body, strlen(body));
    }

    // Perform request
    agentmail_err_t result = AGENTMAIL_ERR_NONE;
    esp_err_t err = esp_http_client_perform(http_client);
    *status_code = esp_http_client_get_status_code(http_client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        result = (err == ESP_ERR_TIMEOUT) ? AGENTMAIL_ERR_TIMEOUT : AGENTMAIL_ERR_NETWORK;
    } else {
        if (client->enable_logging) {
            ESP_LOGI(TAG, "Status: %d, Response size: %zu", *status_code, response->size);
            if (response->size > 0 && response->size < 1024) {
                ESP_LOGD(TAG, "Response: %s", response->buffer);
            }
        }

        // Map HTTP status codes to errors
        if (*status_code >= 200 && *status_code < 300) {
            result = AGENTMAIL_ERR_NONE;
        } else if (*status_code == 401 || *status_code == 403) {
            result = AGENTMAIL_ERR_AUTH;
        } else if (*status_code == 404) {
            result = AGENTMAIL_ERR_NOT_FOUND;
        } else if (*status_code == 429) {
            result = AGENTMAIL_ERR_RATE_LIMIT;
        } else if (*status_code >= 500) {
            result = AGENTMAIL_ERR_SERVER;
        } else {
            result = AGENTMAIL_ERR_OTHER;
        }
    }

    esp_http_client_cleanup(http_client);
    return result;
}

// ============================================================================
// Public API Implementation
// ============================================================================

agentmail_err_t agentmail_init(
    const agentmail_config_t *config,
    agentmail_handle_t *handle
) {
    if (config == NULL || config->api_key == NULL || handle == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)calloc(1, sizeof(agentmail_client_t));
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to allocate client");
        return AGENTMAIL_ERR_NO_MEM;
    }

    client->api_key = strdup(config->api_key);
    client->base_url = strdup(config->base_url ? config->base_url : DEFAULT_BASE_URL);
    client->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : DEFAULT_TIMEOUT_MS;
    client->enable_logging = config->enable_logging;
    client->ctx = config->ctx;

    if (client->api_key == NULL || client->base_url == NULL) {
        free(client->api_key);
        free(client->base_url);
        free(client);
        return AGENTMAIL_ERR_NO_MEM;
    }

    *handle = (agentmail_handle_t)client;
    ESP_LOGI(TAG, "AgentMail client initialized (base: %s)", client->base_url);

    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_destroy(agentmail_handle_t handle) {
    if (handle == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;
    free(client->api_key);
    free(client->base_url);
    free(client);

    ESP_LOGI(TAG, "AgentMail client destroyed");
    return AGENTMAIL_ERR_NONE;
}

// ============================================================================
// Inbox Operations
// ============================================================================

agentmail_err_t agentmail_inbox_create(
    agentmail_handle_t handle,
    const agentmail_inbox_options_t *options,
    agentmail_inbox_t *inbox
) {
    if (handle == NULL || inbox == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;
    memset(inbox, 0, sizeof(agentmail_inbox_t));

    // Build JSON payload
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    if (options != NULL) {
        if (options->name != NULL) {
            cJSON_AddStringToObject(json, "name", options->name);
        }
        if (options->metadata != NULL) {
            cJSON *metadata = cJSON_Parse(options->metadata);
            if (metadata != NULL) {
                cJSON_AddItemToObject(json, "metadata", metadata);
            }
        }
    }

    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (payload == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "POST", "/inboxes", payload, &response, &status_code
    );
    free(payload);

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Parse response
    cJSON *res_json = cJSON_Parse(response.buffer);
    free(response.buffer);
    
    if (res_json == NULL) {
        ESP_LOGE(TAG, "Failed to parse response");
        return AGENTMAIL_ERR_PARSE;
    }

    // Extract inbox info from v0 API response
    cJSON *inbox_id = cJSON_GetObjectItem(res_json, "inbox_id");
    cJSON *address = cJSON_GetObjectItem(res_json, "address");
    cJSON *name = cJSON_GetObjectItem(res_json, "name");
    cJSON *created_at = cJSON_GetObjectItem(res_json, "created_at");
    cJSON *metadata = cJSON_GetObjectItem(res_json, "metadata");

    if (cJSON_IsString(inbox_id)) {
        inbox->inbox_id = strdup(inbox_id->valuestring);
    }
    if (cJSON_IsString(address)) {
        inbox->email_address = strdup(address->valuestring);
    }
    if (cJSON_IsString(name)) {
        inbox->name = strdup(name->valuestring);
    }
    if (cJSON_IsString(created_at)) {
        inbox->created_at = strdup(created_at->valuestring);
    }
    if (cJSON_IsString(metadata)) {
        inbox->metadata = strdup(metadata->valuestring);
    } else if (cJSON_IsObject(metadata)) {
        char *metadata_str = cJSON_PrintUnformatted(metadata);
        if (metadata_str) {
            inbox->metadata = metadata_str;
        }
    }

    cJSON_Delete(res_json);

    if (inbox->inbox_id) {
        ESP_LOGI(TAG, "Created inbox: %s", inbox->inbox_id);
    }

    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_inbox_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    agentmail_inbox_t *inbox
) {
    if (handle == NULL || inbox_id == NULL || inbox == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;
    memset(inbox, 0, sizeof(agentmail_inbox_t));

    // Build path
    char path[256];
    snprintf(path, sizeof(path), "/inboxes/%s", inbox_id);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "GET", path, NULL, &response, &status_code
    );

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Parse response
    cJSON *res_json = cJSON_Parse(response.buffer);
    free(response.buffer);
    
    if (res_json == NULL) {
        return AGENTMAIL_ERR_PARSE;
    }

    // Extract inbox info from v0 API response
    cJSON *inbox_id = cJSON_GetObjectItem(res_json, "inbox_id");
    cJSON *address = cJSON_GetObjectItem(res_json, "address");
    cJSON *name = cJSON_GetObjectItem(res_json, "name");
    cJSON *created_at = cJSON_GetObjectItem(res_json, "created_at");
    cJSON *metadata = cJSON_GetObjectItem(res_json, "metadata");

    if (cJSON_IsString(inbox_id)) {
        inbox->inbox_id = strdup(inbox_id->valuestring);
    }
    if (cJSON_IsString(address)) {
        inbox->email_address = strdup(address->valuestring);
    }
    if (cJSON_IsString(name)) {
        inbox->name = strdup(name->valuestring);
    }
    if (cJSON_IsString(created_at)) {
        inbox->created_at = strdup(created_at->valuestring);
    }
    if (cJSON_IsString(metadata)) {
        inbox->metadata = strdup(metadata->valuestring);
    } else if (cJSON_IsObject(metadata)) {
        char *metadata_str = cJSON_PrintUnformatted(metadata);
        if (metadata_str) {
            inbox->metadata = metadata_str;
        }
    }

    cJSON_Delete(res_json);
    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_inbox_list(
    agentmail_handle_t handle,
    int limit,
    const char *cursor,
    agentmail_inbox_list_t *inboxes
) {
    if (handle == NULL || inboxes == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;
    memset(inboxes, 0, sizeof(agentmail_inbox_list_t));

    // Build path with query params
    char path[512];
    int offset = snprintf(path, sizeof(path), "/inboxes?limit=%d", limit > 0 ? limit : 20);
    if (cursor != NULL) {
        snprintf(path + offset, sizeof(path) - offset, "&cursor=%s", cursor);
    }

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "GET", path, NULL, &response, &status_code
    );

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Parse response (adjust based on actual API response format)
    cJSON *res_json = cJSON_Parse(response.buffer);
    free(response.buffer);
    
    if (res_json == NULL) {
        return AGENTMAIL_ERR_PARSE;
    }

    // v0 API returns array of inboxes directly or in "inboxes" field
    cJSON *data = cJSON_GetObjectItem(res_json, "inboxes");
    if (!data || !cJSON_IsArray(data)) {
        // Try root array
        data = res_json;
    }
    
    if (cJSON_IsArray(data)) {
        size_t count = cJSON_GetArraySize(data);
        if (count > 0) {
            inboxes->inboxes = (agentmail_inbox_t *)calloc(count, sizeof(agentmail_inbox_t));
            if (inboxes->inboxes != NULL) {
                inboxes->count = count;
                for (size_t i = 0; i < count; i++) {
                    cJSON *item = cJSON_GetArrayItem(data, i);
                    cJSON *inbox_id = cJSON_GetObjectItem(item, "inbox_id");
                    cJSON *address = cJSON_GetObjectItem(item, "address");
                    cJSON *name = cJSON_GetObjectItem(item, "name");
                    cJSON *created_at = cJSON_GetObjectItem(item, "created_at");
                    
                    if (cJSON_IsString(inbox_id)) {
                        inboxes->inboxes[i].inbox_id = strdup(inbox_id->valuestring);
                    }
                    if (cJSON_IsString(address)) {
                        inboxes->inboxes[i].email_address = strdup(address->valuestring);
                    }
                    if (cJSON_IsString(name)) {
                        inboxes->inboxes[i].name = strdup(name->valuestring);
                    }
                    if (cJSON_IsString(created_at)) {
                        inboxes->inboxes[i].created_at = strdup(created_at->valuestring);
                    }
                }
            }
        }
    }

    cJSON *next_page_token = cJSON_GetObjectItem(res_json, "next_page_token");
    if (cJSON_IsString(next_page_token)) {
        inboxes->next_cursor = strdup(next_page_token->valuestring);
    }

    cJSON_Delete(res_json);
    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_inbox_update(
    agentmail_handle_t handle,
    const char *inbox_id,
    const agentmail_inbox_options_t *options
) {
    if (handle == NULL || inbox_id == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;

    // Build JSON payload
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    if (options != NULL) {
        if (options->name != NULL) {
            cJSON_AddStringToObject(json, "name", options->name);
        }
        if (options->metadata != NULL) {
            cJSON *metadata = cJSON_Parse(options->metadata);
            if (metadata != NULL) {
                cJSON_AddItemToObject(json, "metadata", metadata);
            }
        }
    }

    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (payload == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    // Build path
    char path[256];
    snprintf(path, sizeof(path), "/inboxes/%s", inbox_id);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "PATCH", path, payload, &response, &status_code
    );

    free(payload);
    free(response.buffer);
    
    if (err == AGENTMAIL_ERR_NONE) {
        ESP_LOGI(TAG, "Updated inbox: %s", inbox_id);
    }

    return err;
}

agentmail_err_t agentmail_inbox_delete(
    agentmail_handle_t handle,
    const char *inbox_id
) {
    if (handle == NULL || inbox_id == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;

    // Build path
    char path[256];
    snprintf(path, sizeof(path), "/inboxes/%s", inbox_id);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "DELETE", path, NULL, &response, &status_code
    );

    free(response.buffer);
    
    if (err == AGENTMAIL_ERR_NONE) {
        ESP_LOGI(TAG, "Deleted inbox: %s", inbox_id);
    }

    return err;
}

// ============================================================================
// Message Operations
// ============================================================================

agentmail_err_t agentmail_send(
    agentmail_handle_t handle,
    const agentmail_send_options_t *options,
    char **message_id
) {
    if (handle == NULL || options == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }
    if (options->from == NULL || options->to == NULL) {
        ESP_LOGE(TAG, "from and to are required");
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;

    // Build JSON payload
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "from", options->from);
    cJSON_AddStringToObject(json, "to", options->to);
    
    if (options->subject != NULL) {
        cJSON_AddStringToObject(json, "subject", options->subject);
    }
    if (options->body_text != NULL) {
        cJSON_AddStringToObject(json, "body_text", options->body_text);
    }
    if (options->body_html != NULL) {
        cJSON_AddStringToObject(json, "body_html", options->body_html);
    }
    if (options->thread_id != NULL) {
        cJSON_AddStringToObject(json, "thread_id", options->thread_id);
    }
    if (options->reply_to != NULL) {
        cJSON_AddStringToObject(json, "reply_to", options->reply_to);
    }

    // Add CC recipients
    if (options->cc != NULL && options->cc_count > 0) {
        cJSON *cc_array = cJSON_CreateArray();
        for (size_t i = 0; i < options->cc_count; i++) {
            cJSON_AddItemToArray(cc_array, cJSON_CreateString(options->cc[i]));
        }
        cJSON_AddItemToObject(json, "cc", cc_array);
    }

    // Add BCC recipients
    if (options->bcc != NULL && options->bcc_count > 0) {
        cJSON *bcc_array = cJSON_CreateArray();
        for (size_t i = 0; i < options->bcc_count; i++) {
            cJSON_AddItemToArray(bcc_array, cJSON_CreateString(options->bcc[i]));
        }
        cJSON_AddItemToObject(json, "bcc", bcc_array);
    }

    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (payload == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    // Build v0 API path: /inboxes/:inbox_id/messages/send
    char path[512];
    snprintf(path, sizeof(path), "/inboxes/%s/messages/send", options->from);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "POST", path, payload, &response, &status_code
    );
    free(payload);

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Parse response to get message ID (v0 API)
    if (message_id != NULL) {
        cJSON *res_json = cJSON_Parse(response.buffer);
        if (res_json != NULL) {
            cJSON *msg_id = cJSON_GetObjectItem(res_json, "message_id");
            if (cJSON_IsString(msg_id)) {
                *message_id = strdup(msg_id->valuestring);
                ESP_LOGI(TAG, "Sent message: %s", *message_id);
            }
            cJSON_Delete(res_json);
        }
    }

    free(response.buffer);
    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_messages_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    const agentmail_message_query_t *query,
    agentmail_message_list_t *messages
) {
    if (handle == NULL || inbox_id == NULL || messages == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;
    memset(messages, 0, sizeof(agentmail_message_list_t));

    // Build path with query params
    char path[1024];
    int limit = (query != NULL && query->limit > 0) ? query->limit : 20;
    int offset = snprintf(path, sizeof(path), "/inboxes/%s/messages?limit=%d", 
                          inbox_id, limit);
    
    if (query != NULL) {
        if (query->cursor != NULL) {
            offset += snprintf(path + offset, sizeof(path) - offset, 
                               "&cursor=%s", query->cursor);
        }
        if (query->unread_only) {
            offset += snprintf(path + offset, sizeof(path) - offset, 
                               "&unread=true");
        }
        if (query->thread_id != NULL) {
            offset += snprintf(path + offset, sizeof(path) - offset, 
                               "&thread_id=%s", query->thread_id);
        }
    }

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "GET", path, NULL, &response, &status_code
    );

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Parse response
    cJSON *res_json = cJSON_Parse(response.buffer);
    free(response.buffer);
    
    if (res_json == NULL) {
        return AGENTMAIL_ERR_PARSE;
    }

    // v0 API returns array of messages in "messages" field
    cJSON *data = cJSON_GetObjectItem(res_json, "messages");
    if (!data || !cJSON_IsArray(data)) {
        // Try root array
        data = res_json;
    }
    
    if (cJSON_IsArray(data)) {
        size_t count = cJSON_GetArraySize(data);
        if (count > 0) {
            messages->messages = (agentmail_message_t *)calloc(count, sizeof(agentmail_message_t));
            if (messages->messages != NULL) {
                messages->count = count;
                for (size_t i = 0; i < count; i++) {
                    cJSON *item = cJSON_GetArrayItem(data, i);
                    agentmail_message_t *msg = &messages->messages[i];
                    
                    cJSON *message_id = cJSON_GetObjectItem(item, "message_id");
                    cJSON *thread_id = cJSON_GetObjectItem(item, "thread_id");
                    cJSON *from = cJSON_GetObjectItem(item, "from");
                    cJSON *to = cJSON_GetObjectItem(item, "to");
                    cJSON *subject = cJSON_GetObjectItem(item, "subject");
                    cJSON *text = cJSON_GetObjectItem(item, "text");
                    cJSON *html = cJSON_GetObjectItem(item, "html");
                    cJSON *created_at = cJSON_GetObjectItem(item, "created_at");
                    cJSON *is_read = cJSON_GetObjectItem(item, "is_read");
                    cJSON *labels = cJSON_GetObjectItem(item, "labels");
                    
                    if (cJSON_IsString(message_id)) msg->message_id = strdup(message_id->valuestring);
                    if (cJSON_IsString(thread_id)) msg->thread_id = strdup(thread_id->valuestring);
                    if (cJSON_IsString(from)) msg->from = strdup(from->valuestring);
                    if (cJSON_IsString(to)) msg->to = strdup(to->valuestring);
                    if (cJSON_IsString(subject)) msg->subject = strdup(subject->valuestring);
                    if (cJSON_IsString(text)) msg->body_text = strdup(text->valuestring);
                    if (cJSON_IsString(html)) msg->body_html = strdup(html->valuestring);
                    if (cJSON_IsString(created_at)) msg->timestamp = strdup(created_at->valuestring);
                    if (cJSON_IsBool(is_read)) msg->is_read = cJSON_IsTrue(is_read);
                }
            }
        }
    }

    cJSON *next_page_token = cJSON_GetObjectItem(res_json, "next_page_token");
    if (cJSON_IsString(next_page_token)) {
        messages->next_cursor = strdup(next_page_token->valuestring);
    }

    cJSON *count = cJSON_GetObjectItem(res_json, "count");
    if (cJSON_IsNumber(count)) {
        messages->total = count->valueint;
    }

    cJSON_Delete(res_json);
    
    ESP_LOGI(TAG, "Retrieved %zu messages from inbox %s", messages->count, inbox_id);
    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_message_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    agentmail_message_t *message
) {
    if (handle == NULL || inbox_id == NULL || message_id == NULL || message == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;
    memset(message, 0, sizeof(agentmail_message_t));

    // Build path
    char path[512];
    snprintf(path, sizeof(path), "/inboxes/%s/messages/%s", inbox_id, message_id);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "GET", path, NULL, &response, &status_code
    );

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Parse response
    cJSON *res_json = cJSON_Parse(response.buffer);
    free(response.buffer);
    
    if (res_json == NULL) {
        return AGENTMAIL_ERR_PARSE;
    }

    // Extract message info (v0 API)
    cJSON *message_id = cJSON_GetObjectItem(res_json, "message_id");
    cJSON *thread_id = cJSON_GetObjectItem(res_json, "thread_id");
    cJSON *from = cJSON_GetObjectItem(res_json, "from");
    cJSON *to = cJSON_GetObjectItem(res_json, "to");
    cJSON *subject = cJSON_GetObjectItem(res_json, "subject");
    cJSON *text = cJSON_GetObjectItem(res_json, "text");
    cJSON *html = cJSON_GetObjectItem(res_json, "html");
    cJSON *created_at = cJSON_GetObjectItem(res_json, "created_at");
    cJSON *is_read = cJSON_GetObjectItem(res_json, "is_read");
    
    if (cJSON_IsString(message_id)) message->message_id = strdup(message_id->valuestring);
    if (cJSON_IsString(thread_id)) message->thread_id = strdup(thread_id->valuestring);
    if (cJSON_IsString(from)) message->from = strdup(from->valuestring);
    if (cJSON_IsString(to)) message->to = strdup(to->valuestring);
    if (cJSON_IsString(subject)) message->subject = strdup(subject->valuestring);
    if (cJSON_IsString(text)) message->body_text = strdup(text->valuestring);
    if (cJSON_IsString(html)) message->body_html = strdup(html->valuestring);
    if (cJSON_IsString(created_at)) message->timestamp = strdup(created_at->valuestring);
    if (cJSON_IsBool(is_read)) message->is_read = cJSON_IsTrue(is_read);

    cJSON_Delete(res_json);
    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_message_mark_read(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    bool is_read
) {
    if (handle == NULL || inbox_id == NULL || message_id == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;

    // Build path
    char path[512];
    snprintf(path, sizeof(path), "/inboxes/%s/messages/%s", inbox_id, message_id);

    // Build JSON payload
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "is_read", is_read);
    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    // Perform request (PATCH or PUT depending on API)
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "PATCH", path, payload, &response, &status_code
    );

    free(payload);
    free(response.buffer);
    
    if (err == AGENTMAIL_ERR_NONE) {
        ESP_LOGI(TAG, "Marked message %s as %s", message_id, is_read ? "read" : "unread");
    }

    return err;
}

agentmail_err_t agentmail_message_delete(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id
) {
    if (handle == NULL || inbox_id == NULL || message_id == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;

    // Build path
    char path[512];
    snprintf(path, sizeof(path), "/inboxes/%s/messages/%s", inbox_id, message_id);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "DELETE", path, NULL, &response, &status_code
    );

    free(response.buffer);
    
    if (err == AGENTMAIL_ERR_NONE) {
        ESP_LOGI(TAG, "Deleted message: %s", message_id);
    }

    return err;
}

agentmail_err_t agentmail_send_reply(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    const agentmail_send_options_t *options,
    char **reply_message_id
) {
    if (handle == NULL || inbox_id == NULL || message_id == NULL || options == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;

    // Build JSON payload
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    // Add reply fields
    if (options->to != NULL) {
        cJSON_AddStringToObject(json, "to", options->to);
    }
    if (options->subject != NULL) {
        cJSON_AddStringToObject(json, "subject", options->subject);
    }
    if (options->body_text != NULL) {
        cJSON_AddStringToObject(json, "text", options->body_text);
    }
    if (options->body_html != NULL) {
        cJSON_AddStringToObject(json, "html", options->body_html);
    }

    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (payload == NULL) {
        return AGENTMAIL_ERR_NO_MEM;
    }

    // Build v0 API path: /inboxes/:inbox_id/messages/:message_id/reply
    char path[512];
    snprintf(path, sizeof(path), "/inboxes/%s/messages/%s/reply", inbox_id, message_id);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "POST", path, payload, &response, &status_code
    );
    free(payload);

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Parse response to get message ID
    if (reply_message_id != NULL) {
        cJSON *res_json = cJSON_Parse(response.buffer);
        if (res_json != NULL) {
            cJSON *msg_id = cJSON_GetObjectItem(res_json, "message_id");
            if (cJSON_IsString(msg_id)) {
                *reply_message_id = strdup(msg_id->valuestring);
                ESP_LOGI(TAG, "Sent reply: %s", *reply_message_id);
            }
            cJSON_Delete(res_json);
        }
    }

    free(response.buffer);
    return AGENTMAIL_ERR_NONE;
}

agentmail_err_t agentmail_message_get_raw(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    char **raw_content,
    size_t *raw_size
) {
    if (handle == NULL || inbox_id == NULL || message_id == NULL || 
        raw_content == NULL || raw_size == NULL) {
        return AGENTMAIL_ERR_INVALID_ARG;
    }

    agentmail_client_t *client = (agentmail_client_t *)handle;

    // Build path
    char path[512];
    snprintf(path, sizeof(path), "/inboxes/%s/messages/%s/raw", inbox_id, message_id);

    // Perform request
    http_response_t response = {};
    int status_code = 0;
    agentmail_err_t err = perform_http_request(
        client, "GET", path, NULL, &response, &status_code
    );

    if (err != AGENTMAIL_ERR_NONE) {
        free(response.buffer);
        return err;
    }

    // Return the raw content
    *raw_content = response.buffer;
    *raw_size = response.size;
    
    ESP_LOGI(TAG, "Retrieved raw message: %s (%zu bytes)", message_id, response.size);
    return AGENTMAIL_ERR_NONE;
}

// ============================================================================
// Memory Management
// ============================================================================

void agentmail_inbox_free(agentmail_inbox_t *inbox) {
    if (inbox == NULL) return;
    
    free(inbox->inbox_id);
    free(inbox->name);
    free(inbox->email_address);
    free(inbox->created_at);
    free(inbox->metadata);
    
    memset(inbox, 0, sizeof(agentmail_inbox_t));
}

void agentmail_inbox_list_free(agentmail_inbox_list_t *list) {
    if (list == NULL) return;
    
    if (list->inboxes != NULL) {
        for (size_t i = 0; i < list->count; i++) {
            agentmail_inbox_free(&list->inboxes[i]);
        }
        free(list->inboxes);
    }
    
    free(list->next_cursor);
    memset(list, 0, sizeof(agentmail_inbox_list_t));
}

void agentmail_message_free(agentmail_message_t *message) {
    if (message == NULL) return;
    
    free(message->message_id);
    free(message->thread_id);
    free(message->from);
    free(message->to);
    free(message->subject);
    free(message->body_text);
    free(message->body_html);
    free(message->timestamp);
    
    if (message->attachments != NULL) {
        for (size_t i = 0; i < message->attachment_count; i++) {
            free(message->attachments[i]);
        }
        free(message->attachments);
    }
    
    memset(message, 0, sizeof(agentmail_message_t));
}

void agentmail_message_list_free(agentmail_message_list_t *list) {
    if (list == NULL) return;
    
    if (list->messages != NULL) {
        for (size_t i = 0; i < list->count; i++) {
            agentmail_message_free(&list->messages[i]);
        }
        free(list->messages);
    }
    
    free(list->next_cursor);
    memset(list, 0, sizeof(agentmail_message_list_t));
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* agentmail_err_to_str(agentmail_err_t err) {
    switch (err) {
        case AGENTMAIL_ERR_NONE:        return "No error";
        case AGENTMAIL_ERR_INVALID_ARG: return "Invalid argument";
        case AGENTMAIL_ERR_NO_MEM:      return "Out of memory";
        case AGENTMAIL_ERR_HTTP:        return "HTTP client error";
        case AGENTMAIL_ERR_AUTH:        return "Authentication failed (401/403)";
        case AGENTMAIL_ERR_PARSE:       return "JSON parse error";
        case AGENTMAIL_ERR_NOT_FOUND:   return "Resource not found (404)";
        case AGENTMAIL_ERR_RATE_LIMIT:  return "Rate limit exceeded (429)";
        case AGENTMAIL_ERR_SERVER:      return "Server error (5xx)";
        case AGENTMAIL_ERR_NETWORK:     return "Network error";
        case AGENTMAIL_ERR_TIMEOUT:     return "Request timeout";
        case AGENTMAIL_ERR_OTHER:       return "Unknown error";
        default:                        return "Invalid error code";
    }
}

