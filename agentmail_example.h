#ifndef AGENTMAIL_EXAMPLE_H
#define AGENTMAIL_EXAMPLE_H

/**
 * @file agentmail_example.h
 * @brief Example usage of AgentMail REST API Client
 * 
 * This file demonstrates how to integrate the AgentMail client
 * into your application. Copy and modify as needed.
 */

#include "agentmail.h"
#include <esp_log.h>
#include <string>
#include <functional>

namespace agentmail {

/**
 * @brief Example class for managing AgentMail operations
 */
class AgentMailManager {
public:
    AgentMailManager() : client_(nullptr), inbox_id_("") {}
    
    ~AgentMailManager() {
        if (client_) {
            agentmail_destroy(client_);
        }
    }
    
    /**
     * @brief Initialize the AgentMail client
     * @param api_key Your AgentMail API key
     * @return true on success
     */
    bool Initialize(const std::string& api_key) {
        if (api_key.empty()) {
            ESP_LOGE(TAG, "API key is empty");
            return false;
        }
        
        agentmail_config_t config = {
            .api_key = api_key.c_str(),
            .base_url = nullptr,  // Use default
            .timeout_ms = 10000,
            .enable_logging = true,
            .ctx = this
        };
        
        agentmail_err_t err = agentmail_init(&config, &client_);
        if (err != AGENTMAIL_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to initialize AgentMail: %s", 
                     agentmail_err_to_str(err));
            return false;
        }
        
        ESP_LOGI(TAG, "AgentMail client initialized");
        return true;
    }
    
    /**
     * @brief Create or get inbox ID
     * @param device_name Name for the inbox
     * @return Inbox ID string (empty on error)
     */
    std::string GetOrCreateInbox(const std::string& device_name) {
        if (!inbox_id_.empty()) {
            return inbox_id_;
        }
        
        // Try to load from settings first
        // inbox_id_ = settings.GetString("agentmail_inbox");
        // if (!inbox_id_.empty()) return inbox_id_;
        
        // Create new inbox
        agentmail_inbox_options_t opts = {
            .name = device_name.c_str(),
            .metadata = nullptr
        };
        
        agentmail_inbox_t inbox = {};
        agentmail_err_t err = agentmail_inbox_create(client_, &opts, &inbox);
        
        if (err == AGENTMAIL_ERR_NONE && inbox.inbox_id) {
            inbox_id_ = inbox.inbox_id;
            ESP_LOGI(TAG, "Created inbox: %s (%s)", 
                     inbox_id_.c_str(), 
                     inbox.email_address ? inbox.email_address : "");
            
            // Save to settings
            // settings.SetString("agentmail_inbox", inbox_id_);
            
            agentmail_inbox_free(&inbox);
            return inbox_id_;
        }
        
        ESP_LOGE(TAG, "Failed to create inbox: %s", agentmail_err_to_str(err));
        agentmail_inbox_free(&inbox);
        return "";
    }
    
    /**
     * @brief Send an email message
     * @param to Recipient email address
     * @param subject Email subject
     * @param body Email body
     * @return true on success
     */
    bool SendMessage(const std::string& to, 
                     const std::string& subject,
                     const std::string& body) {
        if (inbox_id_.empty()) {
            ESP_LOGE(TAG, "No inbox ID set");
            return false;
        }
        
        agentmail_send_options_t opts = {
            .from = inbox_id_.c_str(),
            .to = to.c_str(),
            .subject = subject.c_str(),
            .body_text = body.c_str(),
            .body_html = nullptr,
            .thread_id = nullptr,
            .reply_to = nullptr,
            .cc = nullptr,
            .cc_count = 0,
            .bcc = nullptr,
            .bcc_count = 0
        };
        
        char *message_id = nullptr;
        agentmail_err_t err = agentmail_send(client_, &opts, &message_id);
        
        if (err == AGENTMAIL_ERR_NONE) {
            ESP_LOGI(TAG, "Sent message: %s", message_id ? message_id : "unknown");
            free(message_id);
            return true;
        }
        
        ESP_LOGE(TAG, "Failed to send message: %s", agentmail_err_to_str(err));
        free(message_id);
        return false;
    }
    
    /**
     * @brief Check for new messages
     * @param callback Function to call for each unread message
     * @return Number of unread messages found
     */
    int CheckMessages(std::function<void(const agentmail_message_t&)> callback) {
        if (inbox_id_.empty()) {
            ESP_LOGE(TAG, "No inbox ID set");
            return 0;
        }
        
        agentmail_message_query_t query = {
            .limit = 10,
            .cursor = nullptr,
            .unread_only = true,
            .thread_id = nullptr
        };
        
        agentmail_message_list_t messages = {};
        agentmail_err_t err = agentmail_messages_get(client_, inbox_id_.c_str(), 
                                                      &query, &messages);
        
        if (err != AGENTMAIL_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to get messages: %s", agentmail_err_to_str(err));
            return 0;
        }
        
        ESP_LOGI(TAG, "Retrieved %zu unread messages", messages.count);
        
        for (size_t i = 0; i < messages.count; i++) {
            if (callback) {
                callback(messages.messages[i]);
            }
            
            // Mark as read
            agentmail_message_mark_read(client_, inbox_id_.c_str(),
                                        messages.messages[i].message_id, true);
        }
        
        int count = messages.count;
        agentmail_message_list_free(&messages);
        
        return count;
    }
    
    /**
     * @brief Get inbox ID
     * @return Current inbox ID
     */
    const std::string& GetInboxId() const {
        return inbox_id_;
    }

private:
    static constexpr const char* TAG = "AgentMailManager";
    agentmail_handle_t client_;
    std::string inbox_id_;
};

} // namespace agentmail

#endif // AGENTMAIL_EXAMPLE_H

