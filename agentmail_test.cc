/**
 * AgentMail REST API Console Test Mode
 * 
 * Comprehensive functional test with detailed console output.
 * Tests all major API operations and provides troubleshooting information.
 */

#include "agentmail_test.h"
#include "agentmail.h"
#include "agentmail_example.h"
#include "board.h"
#include "display/display.h"
#include "system_info.h"
#include "wifi_station.h"
#include "ssid_manager.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>

static const char* TAG = "AgentMailTest";

// Test statistics
static struct {
    int messages_sent;
    int messages_received;
    int errors;
    int64_t last_check_time;
    int check_count;
} test_stats = {0, 0, 0, 0, 0};

static void print_test_header() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   AGENTMAIL REST API TEST MODE            ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "This test will:");
    ESP_LOGI(TAG, "  1. Initialize AgentMail client");
    ESP_LOGI(TAG, "  2. Validate API key");
    ESP_LOGI(TAG, "  3. Create/retrieve inbox");
    ESP_LOGI(TAG, "  4. Send test message");
    ESP_LOGI(TAG, "  5. Poll for new messages every %d seconds", 
             CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Requirements:");
    ESP_LOGI(TAG, "  ✓ WiFi connected");
    ESP_LOGI(TAG, "  ✓ Valid AgentMail API key");
    ESP_LOGI(TAG, "  ✓ Internet access");
    ESP_LOGI(TAG, "");
}

static void print_message_details(const agentmail_message_t& msg, int index) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "  Message #%d", index);
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "  Message ID: %s", msg.message_id ? msg.message_id : "N/A");
    ESP_LOGI(TAG, "  From: %s", msg.from ? msg.from : "N/A");
    ESP_LOGI(TAG, "  To: %s", msg.to ? msg.to : "N/A");
    ESP_LOGI(TAG, "  Subject: %s", msg.subject ? msg.subject : "(no subject)");
    ESP_LOGI(TAG, "  Timestamp: %s", msg.timestamp ? msg.timestamp : "N/A");
    ESP_LOGI(TAG, "  Status: %s", msg.is_read ? "Read" : "Unread");
    
    // Print body preview (first 100 chars)
    if (msg.body_text) {
        std::string body = msg.body_text;
        if (body.length() > 100) {
            body = body.substr(0, 100) + "...";
        }
        ESP_LOGI(TAG, "  Body: %s", body.c_str());
    } else {
        ESP_LOGI(TAG, "  Body: (empty)");
    }
    
    if (msg.thread_id) {
        ESP_LOGI(TAG, "  Thread ID: %s", msg.thread_id);
    }
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
}

static void print_statistics() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "┌─────────────────────────────────────────┐");
    ESP_LOGI(TAG, "│  TEST STATISTICS                        │");
    ESP_LOGI(TAG, "└─────────────────────────────────────────┘");
    ESP_LOGI(TAG, "  Messages Sent: %d", test_stats.messages_sent);
    ESP_LOGI(TAG, "  Messages Received: %d", test_stats.messages_received);
    ESP_LOGI(TAG, "  API Checks: %d", test_stats.check_count);
    ESP_LOGI(TAG, "  Errors: %d", test_stats.errors);
    ESP_LOGI(TAG, "");
}

static void check_messages_task(void* arg) {
    agentmail::AgentMailManager* manager = (agentmail::AgentMailManager*)arg;
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL * 1000));
        
        test_stats.check_count++;
        test_stats.last_check_time = esp_timer_get_time() / 1000000;
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "┌─────────────────────────────────────────┐");
        ESP_LOGI(TAG, "│  CHECK #%d (%ds interval)                ", 
                 test_stats.check_count, CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL);
        ESP_LOGI(TAG, "└─────────────────────────────────────────┘");
        ESP_LOGI(TAG, "Checking for new messages...");
        
        int msg_count = 0;
        int new_unread = 0;
        
        msg_count = manager->CheckMessages([&new_unread](const agentmail_message_t& msg) {
            new_unread++;
            print_message_details(msg, new_unread);
        });
        
        if (msg_count > 0) {
            test_stats.messages_received += msg_count;
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "✓ Found %d new message%s", 
                     msg_count, msg_count == 1 ? "" : "s");
        } else {
            ESP_LOGI(TAG, "  (No new messages)");
        }
        
        print_statistics();
    }
}

void start_agentmail_test() {
    print_test_header();
    
    // Get board instance
    auto& board = Board::GetInstance();
    
    ESP_LOGI(TAG, "Device Information:");
    ESP_LOGI(TAG, "  Board: %s", board.GetBoardType().c_str());
    ESP_LOGI(TAG, "  UUID: %s", board.GetUuid().c_str());
    ESP_LOGI(TAG, "  MAC: %s", SystemInfo::GetMacAddress().c_str());
    ESP_LOGI(TAG, "");
    
    // Initialize WiFi/Network
    ESP_LOGI(TAG, "Pre-flight checks:");
    ESP_LOGI(TAG, "  Connecting to WiFi...");
    
    // Check if WiFi credentials are configured
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    
    if (ssid_list.empty()) {
        ESP_LOGE(TAG, "❌ No WiFi configured!");
        ESP_LOGE(TAG, "  Please configure WiFi first using normal app mode.");
        ESP_LOGE(TAG, "Test FAILED. Device will restart in 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    // Let WifiStation handle all WiFi initialization
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.Start();
    
    if (!wifi_station.WaitForConnected(60 * 1000)) {
        ESP_LOGE(TAG, "❌ WiFi connection failed!");
        ESP_LOGE(TAG, "  Check credentials and network availability.");
        ESP_LOGE(TAG, "Test FAILED. Device will restart in 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    ESP_LOGI(TAG, "  ✓ WiFi connected");
    ESP_LOGI(TAG, "");
    
    // Step 1: Initialize AgentMail client
    ESP_LOGI(TAG, "Step 1: Initializing AgentMail client...");
    
#ifdef CONFIG_AGENTMAIL_API_KEY
    std::string api_key = CONFIG_AGENTMAIL_API_KEY;
#else
    std::string api_key = "";
#endif
    
    if (api_key.empty()) {
        ESP_LOGE(TAG, "❌ FAILED: No API key configured!");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Please configure API key:");
        ESP_LOGE(TAG, "  idf.py menuconfig");
        ESP_LOGE(TAG, "    → AgentMail Configuration");
        ESP_LOGE(TAG, "    → AgentMail API Key");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Test FAILED. Device will restart in 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    ESP_LOGI(TAG, "  API Key: %s...%s (length: %d)", 
             api_key.substr(0, 8).c_str(),
             api_key.substr(api_key.length() - 4).c_str(),
             api_key.length());
    
    // Create AgentMail manager
    static agentmail::AgentMailManager agentmail_manager;
    
    if (!agentmail_manager.Initialize(api_key)) {
        test_stats.errors++;
        ESP_LOGE(TAG, "❌ FAILED to initialize AgentMail client!");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Possible causes:");
        ESP_LOGE(TAG, "  - Invalid API key");
        ESP_LOGE(TAG, "  - Network connectivity issues");
        ESP_LOGE(TAG, "  - API endpoint unavailable");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Check logs above for error details.");
        ESP_LOGE(TAG, "Test FAILED. Device will restart in 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    ESP_LOGI(TAG, "  ✓ Client initialized");
    ESP_LOGI(TAG, "  ✓ Connected to: https://api.agentmail.to/v0");
    ESP_LOGI(TAG, "");
    
    // Step 2: Create or get inbox
    ESP_LOGI(TAG, "Step 2: Setting up inbox...");
    
    std::string device_name = "PlaiPin-" + board.GetUuid().substr(0, 6);
    std::string inbox_id = agentmail_manager.GetOrCreateInbox(device_name);
    
    if (inbox_id.empty()) {
        test_stats.errors++;
        ESP_LOGE(TAG, "❌ FAILED to create/get inbox!");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Possible causes:");
        ESP_LOGE(TAG, "  - API authentication failed (check API key)");
        ESP_LOGE(TAG, "  - Rate limit exceeded");
        ESP_LOGE(TAG, "  - Network error");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Test FAILED. Device will restart in 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    ESP_LOGI(TAG, "  ✓ Inbox ready");
    ESP_LOGI(TAG, "  Inbox ID: %s", inbox_id.c_str());
    ESP_LOGI(TAG, "  Name: %s", device_name.c_str());
    ESP_LOGI(TAG, "");
    
    // Step 3: Send test message
    ESP_LOGI(TAG, "Step 3: Sending test message...");
    
#ifdef CONFIG_AGENTMAIL_TEST_RECIPIENT
    std::string recipient = CONFIG_AGENTMAIL_TEST_RECIPIENT;
#else
    std::string recipient = "test@example.com";
#endif
    
    ESP_LOGI(TAG, "  To: %s", recipient.c_str());
    ESP_LOGI(TAG, "  From: %s", inbox_id.c_str());
    
    char subject[128];
    snprintf(subject, sizeof(subject), "Test from PlaiPin (%s)", device_name.c_str());
    
    char body[256];
    snprintf(body, sizeof(body), 
             "This is an automated test message from PlaiPin device.\n\n"
             "Device: %s\n"
             "UUID: %s\n"
             "MAC: %s\n"
             "Time: %lld seconds since boot\n\n"
             "AgentMail REST API Test Mode",
             board.GetBoardType().c_str(),
             board.GetUuid().c_str(),
             SystemInfo::GetMacAddress().c_str(),
             esp_timer_get_time() / 1000000);
    
    bool sent = agentmail_manager.SendMessage(recipient, subject, body);
    
    if (sent) {
        test_stats.messages_sent++;
        ESP_LOGI(TAG, "  ✓ Message sent successfully");
        ESP_LOGI(TAG, "  Subject: %s", subject);
    } else {
        test_stats.errors++;
        ESP_LOGE(TAG, "  ✗ Failed to send message");
        ESP_LOGW(TAG, "  Continuing test anyway...");
    }
    ESP_LOGI(TAG, "");
    
    // Show initial statistics
    print_statistics();
    
    // Show what's happening next
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  ENTERING MESSAGE POLLING MODE            ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "The test will now:");
    ESP_LOGI(TAG, "  • Check for new messages every %d seconds", 
             CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL);
    ESP_LOGI(TAG, "  • Display message details when found");
    ESP_LOGI(TAG, "  • Auto-mark messages as read");
    ESP_LOGI(TAG, "  • Show periodic statistics");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "To test message receiving:");
    ESP_LOGI(TAG, "  • Send an email to: %s", inbox_id.c_str());
    ESP_LOGI(TAG, "  • Wait for next check cycle");
    ESP_LOGI(TAG, "  • Message will appear in logs");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Test running... (press RESET to exit)");
    ESP_LOGI(TAG, "");
    
    // Optional: Update display if available
    auto display = board.GetDisplay();
    if (display) {
        ESP_LOGI(TAG, "Display available - showing test info");
        display->SetChatMessage("system", "AgentMail Test Mode");
        display->SetChatMessage("system", ("Inbox: " + inbox_id).c_str());
        display->SetChatMessage("system", 
            ("Checking every " + std::to_string(CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL) + "s").c_str());
    }
    
    // Start message checking task
    ESP_LOGI(TAG, "Starting periodic check task...");
    xTaskCreate(check_messages_task, "agentmail_check", 
                6144, &agentmail_manager, 5, nullptr);
    ESP_LOGI(TAG, "✓ Check task started");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "║  TEST INITIALIZATION COMPLETE            ║");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "");
    
    // Keep main task alive (test runs indefinitely)
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(60000));  // Wake every minute
        
        // Update display with stats if available
        if (display) {
            char status[64];
            snprintf(status, sizeof(status), 
                     "Sent: %d | Received: %d | Errors: %d",
                     test_stats.messages_sent,
                     test_stats.messages_received,
                     test_stats.errors);
            display->SetStatus(status);
        }
        
        // Periodic health check
        int64_t time_since_check = (esp_timer_get_time() / 1000000) - test_stats.last_check_time;
        ESP_LOGI(TAG, "Health: Last check %lld seconds ago, total checks: %d",
                 time_since_check, test_stats.check_count);
    }
}

