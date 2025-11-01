#ifndef AGENTMAIL_TYPES_H
#define AGENTMAIL_TYPES_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AgentMail error codes
 */
typedef enum {
    AGENTMAIL_ERR_NONE           =  0,  ///< No error
    AGENTMAIL_ERR_INVALID_ARG    = -1,  ///< Invalid argument
    AGENTMAIL_ERR_NO_MEM         = -2,  ///< Out of memory
    AGENTMAIL_ERR_HTTP           = -3,  ///< HTTP client error
    AGENTMAIL_ERR_AUTH           = -4,  ///< Authentication failed
    AGENTMAIL_ERR_PARSE          = -5,  ///< JSON parse error
    AGENTMAIL_ERR_NOT_FOUND      = -6,  ///< Resource not found (404)
    AGENTMAIL_ERR_RATE_LIMIT     = -7,  ///< Rate limit exceeded (429)
    AGENTMAIL_ERR_SERVER         = -8,  ///< Server error (5xx)
    AGENTMAIL_ERR_NETWORK        = -9,  ///< Network error
    AGENTMAIL_ERR_TIMEOUT        = -10, ///< Request timeout
    AGENTMAIL_ERR_OTHER          = -11, ///< Other error
} agentmail_err_t;

/**
 * @brief Opaque handle to AgentMail client
 */
typedef void *agentmail_handle_t;

/**
 * @brief Configuration options for AgentMail client
 */
typedef struct {
    const char *api_key;          ///< Required: API key from agentmail.to
    const char *base_url;         ///< Optional: defaults to "https://api.agentmail.to/v0"
    int timeout_ms;               ///< Optional: HTTP timeout in ms (default: 10000)
    bool enable_logging;          ///< Optional: Enable detailed logging (default: true)
    void *ctx;                    ///< Optional: User context for callbacks
} agentmail_config_t;

/**
 * @brief Inbox information
 */
typedef struct {
    char *inbox_id;               ///< Unique inbox ID (e.g., "abc@agentmail.to")
    char *name;                   ///< Display name
    char *email_address;          ///< Full email address
    char *created_at;             ///< ISO 8601 timestamp
    char *metadata;               ///< Optional JSON metadata
} agentmail_inbox_t;

/**
 * @brief Email message
 */
typedef struct {
    char *message_id;             ///< Unique message ID
    char *thread_id;              ///< Thread ID
    char *from;                   ///< Sender address
    char *to;                     ///< Recipient address
    char *subject;                ///< Email subject
    char *body_text;              ///< Email body (plain text)
    char *body_html;              ///< Email body (HTML, optional)
    char *timestamp;              ///< ISO 8601 timestamp
    bool is_read;                 ///< Read status
    char **attachments;           ///< Array of attachment URLs
    size_t attachment_count;      ///< Number of attachments
} agentmail_message_t;

/**
 * @brief Message list for pagination
 */
typedef struct {
    agentmail_message_t *messages; ///< Array of messages
    size_t count;                  ///< Number of messages in array
    char *next_cursor;             ///< Cursor for next page (NULL if no more)
    size_t total;                  ///< Total messages available (if provided by API)
} agentmail_message_list_t;

/**
 * @brief Inbox list for pagination
 */
typedef struct {
    agentmail_inbox_t *inboxes;    ///< Array of inboxes
    size_t count;                  ///< Number of inboxes in array
    char *next_cursor;             ///< Cursor for next page (NULL if no more)
} agentmail_inbox_list_t;

/**
 * @brief Options for sending an email
 */
typedef struct {
    const char *from;             ///< Required: Sender inbox ID
    const char *to;               ///< Required: Recipient email address
    const char *subject;          ///< Optional: Email subject
    const char *body_text;        ///< Optional: Plain text body
    const char *body_html;        ///< Optional: HTML body
    const char *thread_id;        ///< Optional: Reply to thread
    const char *reply_to;         ///< Optional: Reply-to address
    const char **cc;              ///< Optional: CC recipients
    size_t cc_count;              ///< Number of CC recipients
    const char **bcc;             ///< Optional: BCC recipients
    size_t bcc_count;             ///< Number of BCC recipients
} agentmail_send_options_t;

/**
 * @brief Options for creating an inbox
 */
typedef struct {
    const char *name;             ///< Optional: Display name
    const char *metadata;         ///< Optional: JSON metadata
} agentmail_inbox_options_t;

/**
 * @brief Options for retrieving messages
 */
typedef struct {
    int limit;                    ///< Max number of messages (1-100, default: 20)
    const char *cursor;           ///< Pagination cursor (NULL for first page)
    bool unread_only;             ///< Only return unread messages
    const char *thread_id;        ///< Filter by thread ID
} agentmail_message_query_t;

#ifdef __cplusplus
}
#endif

#endif // AGENTMAIL_TYPES_H

