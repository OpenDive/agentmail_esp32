# AgentMail REST API Client

A lightweight C/C++ client library for the AgentMail REST API, designed for ESP32 devices running ESP-IDF 5.5+.

## Features

- ✉️ **Inbox Management**: Create, get, list, and delete inboxes
- 📨 **Message Operations**: Send, receive, read, and delete messages
- 🔒 **Secure**: HTTPS/TLS support with certificate bundle
- 💾 **Memory Efficient**: Careful memory management for embedded systems
- 🚀 **Easy to Use**: Simple C API with comprehensive error handling
- 📝 **Well Documented**: Extensive API documentation and examples

## Requirements

- ESP-IDF 5.4 or later
- ESP32-S3, ESP32-C3, ESP32-C6, or ESP32-P4
- AgentMail API key from [agentmail.to](https://agentmail.to)
- Internet connection (WiFi configured)

## Installation

The AgentMail client is already included in the plaipin-device project. Enable it in menuconfig:

```bash
idf.py menuconfig
```

Navigate to:
```
Xiaozhi Assistant → AgentMail Configuration
  [*] Enable AgentMail Client
  (your_api_key) AgentMail API Key
```

## Quick Start

### 1. Include Headers

```c
#include "agentmail/agentmail.h"
```

### 2. Initialize Client

```c
// Configure client
agentmail_config_t config = {
    .api_key = "your_api_key_here",
    .base_url = NULL,  // Use default (https://api.agentmail.to/v1)
    .timeout_ms = 10000,
    .enable_logging = true,
    .ctx = NULL
};

// Create client
agentmail_handle_t client = NULL;
agentmail_err_t err = agentmail_init(&config, &client);
if (err != AGENTMAIL_ERR_NONE) {
    ESP_LOGE(TAG, "Failed to init: %s", agentmail_err_to_str(err));
    return;
}
```

### 3. Create an Inbox

```c
agentmail_inbox_options_t opts = {
    .name = "PlaiPin Device",
    .metadata = "{\"device_id\":\"abc123\"}"
};

agentmail_inbox_t inbox = {};
err = agentmail_inbox_create(client, &opts, &inbox);
if (err == AGENTMAIL_ERR_NONE) {
    ESP_LOGI(TAG, "Created inbox: %s", inbox.inbox_id);
    ESP_LOGI(TAG, "Email address: %s", inbox.email_address);
    
    // Store inbox ID for later use
    settings.SetString("agentmail_inbox", inbox.inbox_id);
    
    agentmail_inbox_free(&inbox);
}
```

### 4. Send an Email

```c
agentmail_send_options_t send_opts = {
    .from = "abc@agentmail.to",  // Your inbox ID
    .to = "user@example.com",
    .subject = "Hello from PlaiPin!",
    .body_text = "This is a test message from my device.",
    .thread_id = NULL
};

char *message_id = NULL;
err = agentmail_send(client, &send_opts, &message_id);
if (err == AGENTMAIL_ERR_NONE) {
    ESP_LOGI(TAG, "Sent message: %s", message_id);
    free(message_id);
}
```

### 5. Check for New Messages

```c
agentmail_message_query_t query = {
    .limit = 10,
    .cursor = NULL,
    .unread_only = true,
    .thread_id = NULL
};

agentmail_message_list_t messages = {};
err = agentmail_messages_get(client, "abc@agentmail.to", &query, &messages);
if (err == AGENTMAIL_ERR_NONE) {
    ESP_LOGI(TAG, "Retrieved %zu messages", messages.count);
    
    for (size_t i = 0; i < messages.count; i++) {
        agentmail_message_t *msg = &messages.messages[i];
        ESP_LOGI(TAG, "  From: %s", msg->from);
        ESP_LOGI(TAG, "  Subject: %s", msg->subject);
        ESP_LOGI(TAG, "  Body: %s", msg->body_text);
        
        // Process message...
        
        // Mark as read
        agentmail_message_mark_read(client, "abc@agentmail.to", 
                                     msg->message_id, true);
    }
    
    agentmail_message_list_free(&messages);
}
```

### 6. Cleanup

```c
agentmail_destroy(client);
```

## API Reference

### Initialization

#### `agentmail_init`
Initialize the AgentMail client.

```c
agentmail_err_t agentmail_init(
    const agentmail_config_t *config,
    agentmail_handle_t *handle
);
```

#### `agentmail_destroy`
Destroy the client and free resources.

```c
agentmail_err_t agentmail_destroy(agentmail_handle_t handle);
```

### Inbox Operations

#### `agentmail_inbox_create`
Create a new inbox.

```c
agentmail_err_t agentmail_inbox_create(
    agentmail_handle_t handle,
    const agentmail_inbox_options_t *options,
    agentmail_inbox_t *inbox
);
```

#### `agentmail_inbox_get`
Get inbox information.

```c
agentmail_err_t agentmail_inbox_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    agentmail_inbox_t *inbox
);
```

#### `agentmail_inbox_list`
List all inboxes.

```c
agentmail_err_t agentmail_inbox_list(
    agentmail_handle_t handle,
    int limit,
    const char *cursor,
    agentmail_inbox_list_t *inboxes
);
```

#### `agentmail_inbox_delete`
Delete an inbox.

```c
agentmail_err_t agentmail_inbox_delete(
    agentmail_handle_t handle,
    const char *inbox_id
);
```

### Message Operations

#### `agentmail_send`
Send an email.

```c
agentmail_err_t agentmail_send(
    agentmail_handle_t handle,
    const agentmail_send_options_t *options,
    char **message_id
);
```

#### `agentmail_messages_get`
Get messages from inbox.

```c
agentmail_err_t agentmail_messages_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    const agentmail_message_query_t *query,
    agentmail_message_list_t *messages
);
```

#### `agentmail_message_get`
Get a specific message.

```c
agentmail_err_t agentmail_message_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    agentmail_message_t *message
);
```

#### `agentmail_message_mark_read`
Mark message as read/unread.

```c
agentmail_err_t agentmail_message_mark_read(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    bool is_read
);
```

#### `agentmail_message_delete`
Delete a message.

```c
agentmail_err_t agentmail_message_delete(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id
);
```

### Memory Management

Always free allocated structures when done:

```c
void agentmail_inbox_free(agentmail_inbox_t *inbox);
void agentmail_inbox_list_free(agentmail_inbox_list_t *list);
void agentmail_message_free(agentmail_message_t *message);
void agentmail_message_list_free(agentmail_message_list_t *list);
```

## Integration with PlaiPin Device

### Store Inbox ID in Settings

```cpp
// In Board initialization or Application setup
auto& settings = Settings::GetInstance();
std::string inbox_id = settings.GetString("agentmail_inbox");

if (inbox_id.empty()) {
    // Create inbox on first run
    agentmail_inbox_options_t opts = {
        .name = BOARD_NAME,
        .metadata = nullptr
    };
    agentmail_inbox_t inbox = {};
    if (agentmail_inbox_create(client, &opts, &inbox) == AGENTMAIL_ERR_NONE) {
        settings.SetString("agentmail_inbox", inbox.inbox_id);
        inbox_id = inbox.inbox_id;
        agentmail_inbox_free(&inbox);
    }
}
```

### Integration with ESP-NOW Messaging

```cpp
// When pairing with nearby device via ESP-NOW, exchange AgentMail inboxes
void OnEspNowPairingRequest(const uint8_t* mac, const PairingRequestPayload& payload) {
    ESP_LOGI(TAG, "Device %s has AgentMail inbox: %s", 
             payload.device_name, payload.agentmail_inbox);
    
    // Store peer's AgentMail address for remote messaging
    peer_registry.SetAgentMailAddress(mac, payload.agentmail_inbox);
}

// Send message via AgentMail when peer is not nearby
void SendRemoteMessage(const uint8_t* mac, const std::string& message) {
    std::string peer_inbox = peer_registry.GetAgentMailAddress(mac);
    if (peer_inbox.empty()) {
        ESP_LOGW(TAG, "No AgentMail address for peer");
        return;
    }
    
    agentmail_send_options_t opts = {
        .from = my_inbox_id.c_str(),
        .to = peer_inbox.c_str(),
        .subject = "Message from PlaiPin",
        .body_text = message.c_str()
    };
    
    agentmail_send(agentmail_client, &opts, nullptr);
}
```

### Periodic Message Check

```cpp
// Background task to check for new messages
void CheckAgentMailTask(void* arg) {
    while (true) {
        agentmail_message_query_t query = {
            .limit = 10,
            .cursor = nullptr,
            .unread_only = true
        };
        
        agentmail_message_list_t messages = {};
        if (agentmail_messages_get(client, inbox_id.c_str(), &query, &messages) 
            == AGENTMAIL_ERR_NONE) {
            
            for (size_t i = 0; i < messages.count; i++) {
                // Process message (e.g., inject into LLM)
                ProcessIncomingMessage(&messages.messages[i]);
                
                // Mark as read
                agentmail_message_mark_read(client, inbox_id.c_str(),
                    messages.messages[i].message_id, true);
            }
            
            agentmail_message_list_free(&messages);
        }
        
        vTaskDelay(pdMS_TO_TICKS(60000));  // Check every minute
    }
}
```

## Configuration Options (Kconfig)

```
CONFIG_ENABLE_AGENTMAIL           - Enable AgentMail client
CONFIG_AGENTMAIL_API_KEY          - API key from agentmail.to
CONFIG_AGENTMAIL_BASE_URL         - API base URL (default: https://api.agentmail.to/v1)
CONFIG_AGENTMAIL_DEFAULT_TIMEOUT  - HTTP timeout in ms (default: 10000)
CONFIG_AGENTMAIL_MAX_MESSAGE_SIZE - Max message size in bytes (default: 16384)
CONFIG_AGENTMAIL_ENABLE_LOGGING   - Enable detailed logging (default: y)
```

## Error Handling

All functions return `agentmail_err_t`:

```c
typedef enum {
    AGENTMAIL_ERR_NONE = 0,         // Success
    AGENTMAIL_ERR_INVALID_ARG = -1, // Invalid argument
    AGENTMAIL_ERR_NO_MEM = -2,      // Out of memory
    AGENTMAIL_ERR_HTTP = -3,        // HTTP client error
    AGENTMAIL_ERR_AUTH = -4,        // Authentication failed (401/403)
    AGENTMAIL_ERR_PARSE = -5,       // JSON parse error
    AGENTMAIL_ERR_NOT_FOUND = -6,   // Resource not found (404)
    AGENTMAIL_ERR_RATE_LIMIT = -7,  // Rate limit exceeded (429)
    AGENTMAIL_ERR_SERVER = -8,      // Server error (5xx)
    AGENTMAIL_ERR_NETWORK = -9,     // Network error
    AGENTMAIL_ERR_TIMEOUT = -10,    // Request timeout
    AGENTMAIL_ERR_OTHER = -11       // Other error
} agentmail_err_t;
```

Convert to string with `agentmail_err_to_str()`.

## Memory Considerations

- Max HTTP response size: 32KB (configurable)
- Message body size limit: 16KB (configurable)
- Always free returned structures with provided free functions
- Memory is allocated dynamically - monitor heap usage

## Security Notes

1. **API Key Storage**: Store API keys securely in NVS (encrypted partition recommended)
2. **TLS/HTTPS**: Always use HTTPS endpoints (default)
3. **Certificate Validation**: Certificate bundle is included by default
4. **Metadata Privacy**: Don't include sensitive data in metadata fields

## Troubleshooting

### Authentication Errors

```
Error: Authentication failed (401/403)
```
- Check API key is correct
- Verify API key has necessary permissions
- Check API key hasn't expired

### Network Errors

```
Error: Network error / Request timeout
```
- Verify WiFi connection is established
- Check firewall/proxy settings
- Try increasing timeout in config
- Verify DNS resolution works

### Memory Issues

```
Error: Out of memory
```
- Reduce message list query limit
- Free structures immediately after use
- Monitor heap with `esp_get_free_heap_size()`
- Consider increasing `CONFIG_AGENTMAIL_MAX_MESSAGE_SIZE`

### Rate Limiting

```
Error: Rate limit exceeded (429)
```
- Implement exponential backoff
- Reduce polling frequency
- Batch operations when possible

## License

Same as plaipin-device project.

## Support

For AgentMail API documentation, visit: https://docs.agentmail.to

For issues with this client, please file an issue in the plaipin-device repository.

