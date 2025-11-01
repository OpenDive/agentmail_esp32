#ifndef AGENTMAIL_H
#define AGENTMAIL_H

#include "agentmail_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup AgentMail AgentMail REST API Client
 * @brief Client library for AgentMail REST API
 * @{
 */

/**
 * @brief Initialize AgentMail client
 * 
 * Creates a new AgentMail client instance with the provided configuration.
 * 
 * @param[in] config Configuration options (must not be NULL)
 * @param[out] handle Output client handle (must not be NULL)
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note config->api_key must be provided
 * @note Call agentmail_destroy() when done
 * 
 * Example:
 * @code
 * agentmail_config_t config = {
 *     .api_key = "your_api_key",
 *     .base_url = NULL,  // Use default
 *     .timeout_ms = 10000
 * };
 * agentmail_handle_t client;
 * agentmail_err_t err = agentmail_init(&config, &client);
 * @endcode
 */
agentmail_err_t agentmail_init(
    const agentmail_config_t *config,
    agentmail_handle_t *handle
);

/**
 * @brief Destroy AgentMail client
 * 
 * Frees all resources associated with the client.
 * 
 * @param[in] handle Client handle
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 */
agentmail_err_t agentmail_destroy(agentmail_handle_t handle);

/**
 * @defgroup Inbox Inbox Management
 * @brief Operations for managing inboxes
 * @{
 */

/**
 * @brief Create a new inbox
 * 
 * Creates a new inbox with optional name and metadata.
 * 
 * @param[in] handle Client handle
 * @param[in] options Inbox creation options (can be NULL for defaults)
 * @param[out] inbox Output inbox information
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note Call agentmail_inbox_free() when done with inbox
 * 
 * Example:
 * @code
 * agentmail_inbox_options_t opts = {
 *     .name = "PlaiPin Device",
 *     .metadata = "{\"device_id\":\"abc123\"}"
 * };
 * agentmail_inbox_t inbox = {};
 * agentmail_err_t err = agentmail_inbox_create(client, &opts, &inbox);
 * if (err == AGENTMAIL_ERR_NONE) {
 *     ESP_LOGI(TAG, "Created inbox: %s", inbox.inbox_id);
 *     agentmail_inbox_free(&inbox);
 * }
 * @endcode
 */
agentmail_err_t agentmail_inbox_create(
    agentmail_handle_t handle,
    const agentmail_inbox_options_t *options,
    agentmail_inbox_t *inbox
);

/**
 * @brief Get inbox information
 * 
 * Retrieves information about a specific inbox.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID
 * @param[out] inbox Output inbox information
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note Call agentmail_inbox_free() when done with inbox
 */
agentmail_err_t agentmail_inbox_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    agentmail_inbox_t *inbox
);

/**
 * @brief List all inboxes
 * 
 * Retrieves a list of all inboxes with optional pagination.
 * 
 * @param[in] handle Client handle
 * @param[in] limit Max number of inboxes to return (1-100)
 * @param[in] cursor Pagination cursor (NULL for first page)
 * @param[out] inboxes Output inbox list
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note Call agentmail_inbox_list_free() when done
 */
agentmail_err_t agentmail_inbox_list(
    agentmail_handle_t handle,
    int limit,
    const char *cursor,
    agentmail_inbox_list_t *inboxes
);

/**
 * @brief Update an inbox
 * 
 * Updates inbox name and/or metadata.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID to update
 * @param[in] options Update options (can be NULL for no changes)
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 */
agentmail_err_t agentmail_inbox_update(
    agentmail_handle_t handle,
    const char *inbox_id,
    const agentmail_inbox_options_t *options
);

/**
 * @brief Delete an inbox
 * 
 * Permanently deletes an inbox and all its messages.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID to delete
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @warning This operation cannot be undone!
 */
agentmail_err_t agentmail_inbox_delete(
    agentmail_handle_t handle,
    const char *inbox_id
);

/** @} */ // end of Inbox group

/**
 * @defgroup Messages Message Operations
 * @brief Operations for sending and managing messages
 * @{
 */

/**
 * @brief Send an email
 * 
 * Sends an email from one of your inboxes.
 * 
 * @param[in] handle Client handle
 * @param[in] options Send options (must not be NULL)
 * @param[out] message_id Output message ID (optional, can be NULL)
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note options->from and options->to are required
 * @note If message_id is not NULL, caller must free() it
 * 
 * Example:
 * @code
 * agentmail_send_options_t opts = {
 *     .from = "abc@agentmail.to",
 *     .to = "user@example.com",
 *     .subject = "Hello from PlaiPin",
 *     .body_text = "This is a test message",
 *     .thread_id = NULL
 * };
 * char *msg_id = NULL;
 * agentmail_err_t err = agentmail_send(client, &opts, &msg_id);
 * if (err == AGENTMAIL_ERR_NONE) {
 *     ESP_LOGI(TAG, "Sent message: %s", msg_id);
 *     free(msg_id);
 * }
 * @endcode
 */
agentmail_err_t agentmail_send(
    agentmail_handle_t handle,
    const agentmail_send_options_t *options,
    char **message_id
);

/**
 * @brief Reply to an email
 * 
 * Sends a reply to an existing message.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID (sender)
 * @param[in] message_id Original message ID to reply to
 * @param[in] options Send options (must not be NULL)
 * @param[out] reply_message_id Output reply message ID (optional, can be NULL)
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note This automatically sets in_reply_to and thread_id
 */
agentmail_err_t agentmail_send_reply(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    const agentmail_send_options_t *options,
    char **reply_message_id
);

/**
 * @brief Retrieve messages from inbox
 * 
 * Gets a list of messages from the specified inbox with optional filtering.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID
 * @param[in] query Query options (can be NULL for defaults)
 * @param[out] messages Output message list
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note Call agentmail_message_list_free() when done
 * 
 * Example:
 * @code
 * agentmail_message_query_t query = {
 *     .limit = 10,
 *     .cursor = NULL,
 *     .unread_only = true
 * };
 * agentmail_message_list_t messages = {};
 * agentmail_err_t err = agentmail_messages_get(client, "abc@agentmail.to", &query, &messages);
 * if (err == AGENTMAIL_ERR_NONE) {
 *     for (size_t i = 0; i < messages.count; i++) {
 *         ESP_LOGI(TAG, "From: %s, Subject: %s", 
 *                  messages.messages[i].from, messages.messages[i].subject);
 *     }
 *     agentmail_message_list_free(&messages);
 * }
 * @endcode
 */
agentmail_err_t agentmail_messages_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    const agentmail_message_query_t *query,
    agentmail_message_list_t *messages
);

/**
 * @brief Get a specific message
 * 
 * Retrieves a single message by ID.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID
 * @param[in] message_id Message ID
 * @param[out] message Output message
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @note Call agentmail_message_free() when done
 */
agentmail_err_t agentmail_message_get(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    agentmail_message_t *message
);

/**
 * @brief Mark message as read
 * 
 * Updates the read status of a message.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID
 * @param[in] message_id Message ID
 * @param[in] is_read Read status (true = read, false = unread)
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 */
agentmail_err_t agentmail_message_mark_read(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    bool is_read
);

/**
 * @brief Delete a message
 * 
 * Permanently deletes a message.
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID
 * @param[in] message_id Message ID
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 * 
 * @warning This operation cannot be undone!
 */
agentmail_err_t agentmail_message_delete(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id
);

/**
 * @brief Get raw message content
 * 
 * Retrieves the raw email content (MIME format).
 * 
 * @param[in] handle Client handle
 * @param[in] inbox_id Inbox ID
 * @param[in] message_id Message ID
 * @param[out] raw_content Output raw content (caller must free())
 * @param[out] raw_size Output size of raw content
 * @return AGENTMAIL_ERR_NONE on success, error code otherwise
 */
agentmail_err_t agentmail_message_get_raw(
    agentmail_handle_t handle,
    const char *inbox_id,
    const char *message_id,
    char **raw_content,
    size_t *raw_size
);

/** @} */ // end of Messages group

/**
 * @defgroup Memory Memory Management
 * @brief Functions for freeing allocated resources
 * @{
 */

/**
 * @brief Free inbox structure
 * 
 * Frees all memory allocated within an inbox structure.
 * 
 * @param[in] inbox Inbox structure to free
 * 
 * @note Does not free the inbox pointer itself if it was allocated separately
 */
void agentmail_inbox_free(agentmail_inbox_t *inbox);

/**
 * @brief Free inbox list
 * 
 * Frees all memory allocated within an inbox list structure.
 * 
 * @param[in] list Inbox list to free
 */
void agentmail_inbox_list_free(agentmail_inbox_list_t *list);

/**
 * @brief Free message structure
 * 
 * Frees all memory allocated within a message structure.
 * 
 * @param[in] message Message structure to free
 * 
 * @note Does not free the message pointer itself if it was allocated separately
 */
void agentmail_message_free(agentmail_message_t *message);

/**
 * @brief Free message list
 * 
 * Frees all memory allocated within a message list structure.
 * 
 * @param[in] list Message list to free
 */
void agentmail_message_list_free(agentmail_message_list_t *list);

/** @} */ // end of Memory group

/**
 * @defgroup Utilities Utility Functions
 * @brief Helper functions
 * @{
 */

/**
 * @brief Convert error code to string
 * 
 * Returns a human-readable description of an error code.
 * 
 * @param[in] err Error code
 * @return Error string (never NULL)
 */
const char* agentmail_err_to_str(agentmail_err_t err);

/** @} */ // end of Utilities group

/** @} */ // end of AgentMail group

#ifdef __cplusplus
}
#endif

#endif // AGENTMAIL_H

