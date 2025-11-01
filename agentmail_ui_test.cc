/**
 * AgentMail REST API UI Test Mode
 * 
 * Comprehensive functional test with LVGL visual interface.
 * Provides real-time feedback on API operations and message status.
 */

#include "agentmail_ui_test.h"
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
#include <lvgl.h>
#include <algorithm>
#include <string>
#include <vector>

static const char* TAG = "AgentMailUITest";

// UI elements
static lv_obj_t* screen_ = nullptr;
static lv_obj_t* header_label_ = nullptr;
static lv_obj_t* status_label_ = nullptr;
static lv_obj_t* inbox_container_ = nullptr;
static lv_obj_t* inbox_id_label_ = nullptr;
static lv_obj_t* inbox_name_label_ = nullptr;
static lv_obj_t* operation_container_ = nullptr;
static lv_obj_t* operation_label_ = nullptr;
static lv_obj_t* messages_container_ = nullptr;
static lv_obj_t* message_cards_[10] = {nullptr};  // Top 10 messages
static lv_obj_t* stats_label_ = nullptr;

// AgentMail manager (static to persist)
static agentmail::AgentMailManager* agentmail_manager_ = nullptr;

// Test state
static struct {
    int messages_sent;
    int messages_received;
    int errors;
    int64_t last_check_time;
    int check_count;
    std::string inbox_id;
    std::vector<std::string> recent_operations;
} test_state;

// Color scheme (matching proximity test style)
static const lv_color_t COLOR_BG = lv_color_hex(0x0A0E27);
static const lv_color_t COLOR_HEADER = lv_color_hex(0x1A1F3A);
static const lv_color_t COLOR_SECTION = lv_color_hex(0x2A2F4A);
static const lv_color_t COLOR_MESSAGE_BG = lv_color_hex(0x1E2337);
static const lv_color_t COLOR_TEXT = lv_color_hex(0xFFFFFF);
static const lv_color_t COLOR_TEXT_DIM = lv_color_hex(0xAAAAAA);
static const lv_color_t COLOR_SUCCESS = lv_color_hex(0x00FF88);
static const lv_color_t COLOR_WARNING = lv_color_hex(0xFFDD00);
static const lv_color_t COLOR_ERROR = lv_color_hex(0xFF4444);
static const lv_color_t COLOR_ACCENT = lv_color_hex(0x00D9FF);
static const lv_color_t COLOR_UNREAD = lv_color_hex(0xFF8800);

static void create_ui() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    if (!display) {
        ESP_LOGE(TAG, "No display available - cannot create UI");
        return;
    }
    
    ESP_LOGI(TAG, "Creating UI...");
    
    // Lock LVGL for thread safety
    DisplayLockGuard lock(display);
    
    // Create new screen
    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, COLOR_BG, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);
    
    // Header section (60px)
    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_size(header, LV_HOR_RES, 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, COLOR_HEADER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 8, 0);
    
    // Header title
    header_label_ = lv_label_create(header);
    lv_label_set_text(header_label_, LV_SYMBOL_ENVELOPE " AgentMail API Test");
    lv_obj_set_style_text_color(header_label_, COLOR_ACCENT, 0);
    lv_obj_align(header_label_, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Status line
    status_label_ = lv_label_create(header);
    lv_label_set_text(status_label_, "Initializing...");
    lv_obj_set_style_text_color(status_label_, COLOR_SUCCESS, 0);
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // Inbox section (80px)
    inbox_container_ = lv_obj_create(screen_);
    lv_obj_set_size(inbox_container_, LV_HOR_RES - 8, 80);
    lv_obj_align(inbox_container_, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_bg_color(inbox_container_, COLOR_SECTION, 0);
    lv_obj_set_style_border_width(inbox_container_, 1, 0);
    lv_obj_set_style_border_color(inbox_container_, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(inbox_container_, 4, 0);
    lv_obj_set_style_pad_all(inbox_container_, 8, 0);
    
    inbox_id_label_ = lv_label_create(inbox_container_);
    lv_label_set_text(inbox_id_label_, "Inbox: ...");
    lv_obj_set_style_text_color(inbox_id_label_, COLOR_TEXT, 0);
    lv_obj_align(inbox_id_label_, LV_ALIGN_TOP_LEFT, 0, 0);
    
    inbox_name_label_ = lv_label_create(inbox_container_);
    lv_label_set_text(inbox_name_label_, "Name: ...");
    lv_obj_set_style_text_color(inbox_name_label_, COLOR_TEXT_DIM, 0);
    lv_obj_align(inbox_name_label_, LV_ALIGN_TOP_LEFT, 0, 20);
    
    // Last operation section (60px)
    operation_container_ = lv_obj_create(screen_);
    lv_obj_set_size(operation_container_, LV_HOR_RES - 8, 60);
    lv_obj_align(operation_container_, LV_ALIGN_TOP_MID, 0, 148);
    lv_obj_set_style_bg_color(operation_container_, COLOR_SECTION, 0);
    lv_obj_set_style_border_width(operation_container_, 1, 0);
    lv_obj_set_style_border_color(operation_container_, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(operation_container_, 4, 0);
    lv_obj_set_style_pad_all(operation_container_, 8, 0);
    
    operation_label_ = lv_label_create(operation_container_);
    lv_label_set_text(operation_label_, "Last operation: None");
    lv_obj_set_width(operation_label_, lv_pct(100));
    lv_label_set_long_mode(operation_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(operation_label_, COLOR_TEXT_DIM, 0);
    
    // Messages list container (scrollable)
    int messages_y = 212;
    int messages_height = LV_VER_RES - messages_y - 50;  // Leave room for stats
    
    messages_container_ = lv_obj_create(screen_);
    lv_obj_set_size(messages_container_, LV_HOR_RES - 8, messages_height);
    lv_obj_align(messages_container_, LV_ALIGN_TOP_MID, 0, messages_y);
    lv_obj_set_style_bg_color(messages_container_, COLOR_BG, 0);
    lv_obj_set_style_border_width(messages_container_, 0, 0);
    lv_obj_set_style_radius(messages_container_, 0, 0);
    lv_obj_set_style_pad_all(messages_container_, 4, 0);
    lv_obj_set_scrollbar_mode(messages_container_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(messages_container_, LV_DIR_VER);
    lv_obj_set_flex_flow(messages_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(messages_container_, LV_FLEX_ALIGN_START, 
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(messages_container_, 4, 0);
    
    // Create 10 message card slots
    for (int i = 0; i < 10; i++) {
        lv_obj_t* card = lv_obj_create(messages_container_);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, COLOR_MESSAGE_BG, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_radius(card, 4, 0);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
        
        // Create label for message content
        message_cards_[i] = lv_label_create(card);
        lv_obj_set_width(message_cards_[i], lv_pct(100));
        lv_label_set_long_mode(message_cards_[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(message_cards_[i], COLOR_TEXT, 0);
        lv_label_set_text(message_cards_[i], "");
    }
    
    // Stats footer (40px)
    lv_obj_t* stats_bar = lv_obj_create(screen_);
    lv_obj_set_size(stats_bar, LV_HOR_RES, 40);
    lv_obj_align(stats_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(stats_bar, COLOR_HEADER, 0);
    lv_obj_set_style_border_width(stats_bar, 0, 0);
    lv_obj_set_style_radius(stats_bar, 0, 0);
    lv_obj_set_style_pad_all(stats_bar, 6, 0);
    
    stats_label_ = lv_label_create(stats_bar);
    lv_label_set_text(stats_label_, "Sent: 0 | Received: 0 | Errors: 0");
    lv_obj_set_style_text_color(stats_label_, COLOR_TEXT_DIM, 0);
    lv_obj_align(stats_label_, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Load the screen
    lv_screen_load(screen_);
    
    ESP_LOGI(TAG, "UI created successfully");
}

static void update_status(const char* status, lv_color_t color) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    if (!display || !status_label_) return;
    
    DisplayLockGuard lock(display);
    lv_label_set_text(status_label_, status);
    lv_obj_set_style_text_color(status_label_, color, 0);
}

static void update_inbox_display(const std::string& inbox_id, const std::string& name) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    if (!display || !inbox_id_label_) return;
    
    DisplayLockGuard lock(display);
    
    std::string inbox_text = "Inbox: " + inbox_id;
    lv_label_set_text(inbox_id_label_, inbox_text.c_str());
    
    std::string name_text = "Name: " + name;
    lv_label_set_text(inbox_name_label_, name_text.c_str());
    
    // Update section border to success color
    lv_obj_set_style_border_color(inbox_container_, COLOR_SUCCESS, 0);
}

static void update_operation(const std::string& operation, bool success) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    if (!display || !operation_label_) return;
    
    test_state.recent_operations.push_back(operation);
    if (test_state.recent_operations.size() > 3) {
        test_state.recent_operations.erase(test_state.recent_operations.begin());
    }
    
    DisplayLockGuard lock(display);
    
    std::string op_text = success ? (LV_SYMBOL_OK " ") : (LV_SYMBOL_CLOSE " ");
    op_text += operation;
    
    lv_label_set_text(operation_label_, op_text.c_str());
    lv_obj_set_style_text_color(operation_label_, 
                                 success ? COLOR_SUCCESS : COLOR_ERROR, 0);
    lv_obj_set_style_border_color(operation_container_,
                                   success ? COLOR_SUCCESS : COLOR_ERROR, 0);
}

static void update_messages_display() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    if (!display || !agentmail_manager_) return;
    
    // This would be called by the check task after retrieving messages
    // For now, we'll just update with placeholder logic
    
    DisplayLockGuard lock(display);
    
    // In real implementation, messages would be stored and displayed here
    // For now, just hide all cards (they'll be shown when messages arrive)
    for (int i = 0; i < 10; i++) {
        lv_obj_t* card = (lv_obj_t*)lv_obj_get_parent(message_cards_[i]);
        lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_stats_display() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    if (!display || !stats_label_) return;
    
    DisplayLockGuard lock(display);
    
    char stats_text[128];
    int64_t time_since_check = (esp_timer_get_time() / 1000000) - test_state.last_check_time;
    int next_check = CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL - time_since_check;
    if (next_check < 0) next_check = 0;
    
    snprintf(stats_text, sizeof(stats_text),
             "Sent: %d | Received: %d | Errors: %d | Next: %ds",
             test_state.messages_sent,
             test_state.messages_received,
             test_state.errors,
             next_check);
    
    lv_label_set_text(stats_label_, stats_text);
}

static void ui_update_task(void* arg) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
        update_stats_display();
    }
}

static void message_check_task(void* arg) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL * 1000));
        
        if (!agentmail_manager_) continue;
        
        test_state.check_count++;
        test_state.last_check_time = esp_timer_get_time() / 1000000;
        
        ESP_LOGI(TAG, "Checking for messages (check #%d)...", test_state.check_count);
        
        int msg_count = agentmail_manager_->CheckMessages([](const agentmail_message_t& msg) {
            test_state.messages_received++;
            
            ESP_LOGI(TAG, "New message: %s - %s", 
                     msg.from ? msg.from : "unknown",
                     msg.subject ? msg.subject : "(no subject)");
            
            // Update operation display
            std::string op = "Received: ";
            op += msg.subject ? msg.subject : "(no subject)";
            update_operation(op, true);
        });
        
        if (msg_count == 0) {
            ESP_LOGI(TAG, "No new messages");
        }
        
        update_messages_display();
    }
}

void start_agentmail_ui_test() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   AGENTMAIL REST API - UI TEST MODE       ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // Get board instance
    auto& board = Board::GetInstance();
    
    ESP_LOGI(TAG, "Device Information:");
    ESP_LOGI(TAG, "  Board: %s", board.GetBoardType().c_str());
    ESP_LOGI(TAG, "  UUID: %s", board.GetUuid().c_str());
    ESP_LOGI(TAG, "  MAC: %s", SystemInfo::GetMacAddress().c_str());
    ESP_LOGI(TAG, "");
    
    // Initialize WiFi/Network
    ESP_LOGI(TAG, "Connecting to WiFi...");
    
    // Check if WiFi credentials are configured
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    
    if (ssid_list.empty()) {
        ESP_LOGE(TAG, "No WiFi configured!");
        ESP_LOGE(TAG, "Please configure WiFi first using normal app mode.");
        ESP_LOGE(TAG, "Test will restart in 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    // Let WifiStation handle all WiFi initialization
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.Start();
    
    if (!wifi_station.WaitForConnected(60 * 1000)) {
        ESP_LOGE(TAG, "WiFi connection failed!");
        ESP_LOGE(TAG, "Check credentials and network availability.");
        ESP_LOGE(TAG, "Test will restart in 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    ESP_LOGI(TAG, "✓ WiFi connected");
    ESP_LOGI(TAG, "");
    
    // Check for display
    auto display = board.GetDisplay();
    if (!display) {
        ESP_LOGW(TAG, "⚠️  No display available - falling back to console mode");
        ESP_LOGI(TAG, "To use console test mode, select:");
        ESP_LOGI(TAG, "  idf.py menuconfig → Test Modes → AgentMail Test UI Mode → Console Mode");
        ESP_LOGI(TAG, "");
        ESP_LOGW(TAG, "Test cannot continue without display in UI mode.");
        ESP_LOGW(TAG, "Device will restart in 10 seconds...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
        return;
    }
    
    // Wait for display to be ready
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Create UI
    ESP_LOGI(TAG, "Creating UI...");
    create_ui();
    
    if (!screen_) {
        ESP_LOGE(TAG, "Failed to create UI");
        ESP_LOGE(TAG, "Device will restart in 10 seconds...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
        return;
    }
    
    ESP_LOGI(TAG, "✓ UI ready");
    ESP_LOGI(TAG, "");
    
    update_status("Initializing...", COLOR_WARNING);
    
    // Get API key
#ifdef CONFIG_AGENTMAIL_API_KEY
    std::string api_key = CONFIG_AGENTMAIL_API_KEY;
#else
    std::string api_key = "";
#endif
    
    if (api_key.empty()) {
        ESP_LOGE(TAG, "No API key configured!");
        update_status("ERROR: No API key", COLOR_ERROR);
        update_operation("Failed: No API key configured", false);
        test_state.errors++;
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    // Initialize AgentMail
    ESP_LOGI(TAG, "Initializing AgentMail client...");
    update_status("Connecting to API...", COLOR_WARNING);
    
    agentmail_manager_ = new agentmail::AgentMailManager();
    
    if (!agentmail_manager_->Initialize(api_key)) {
        ESP_LOGE(TAG, "Failed to initialize AgentMail client");
        update_status("ERROR: Init failed", COLOR_ERROR);
        update_operation("Failed: Client initialization", false);
        test_state.errors++;
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    ESP_LOGI(TAG, "✓ Client initialized");
    update_status(LV_SYMBOL_OK " Connected", COLOR_SUCCESS);
    update_operation("Client initialized successfully", true);
    
    // Create/get inbox
    ESP_LOGI(TAG, "Setting up inbox...");
    update_status("Setting up inbox...", COLOR_WARNING);
    
    std::string device_name = "PlaiPin-" + board.GetUuid().substr(0, 6);
    std::string inbox_id = agentmail_manager_->GetOrCreateInbox(device_name);
    
    if (inbox_id.empty()) {
        ESP_LOGE(TAG, "Failed to create/get inbox");
        update_status("ERROR: Inbox failed", COLOR_ERROR);
        update_operation("Failed: Inbox creation", false);
        test_state.errors++;
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
        return;
    }
    
    test_state.inbox_id = inbox_id;
    ESP_LOGI(TAG, "✓ Inbox ready: %s", inbox_id.c_str());
    update_status(LV_SYMBOL_OK " Inbox ready", COLOR_SUCCESS);
    update_inbox_display(inbox_id, device_name);
    update_operation("Inbox created: " + inbox_id, true);
    
    // Send test message
    ESP_LOGI(TAG, "Sending test message...");
    update_status("Sending test message...", COLOR_WARNING);
    
#ifdef CONFIG_AGENTMAIL_TEST_RECIPIENT
    std::string recipient = CONFIG_AGENTMAIL_TEST_RECIPIENT;
#else
    std::string recipient = "test@example.com";
#endif
    
    char subject[128];
    snprintf(subject, sizeof(subject), "Test from %s", device_name.c_str());
    
    char body[256];
    snprintf(body, sizeof(body),
             "Automated test message from PlaiPin device.\n"
             "Board: %s\nUUID: %s\nMAC: %s",
             board.GetBoardType().c_str(),
             board.GetUuid().c_str(),
             SystemInfo::GetMacAddress().c_str());
    
    bool sent = agentmail_manager_->SendMessage(recipient, subject, body);
    
    if (sent) {
        test_state.messages_sent++;
        ESP_LOGI(TAG, "✓ Message sent to %s", recipient.c_str());
        update_operation("Sent test message to " + recipient, true);
    } else {
        test_state.errors++;
        ESP_LOGE(TAG, "Failed to send message");
        update_operation("Failed to send test message", false);
    }
    
    update_status(LV_SYMBOL_OK " Test complete", COLOR_SUCCESS);
    
    // Start background tasks
    ESP_LOGI(TAG, "Starting background tasks...");
    xTaskCreate(ui_update_task, "agentmail_ui_update", 
                4096, nullptr, 5, nullptr);
    xTaskCreate(message_check_task, "agentmail_check", 
                6144, nullptr, 5, nullptr);
    
    test_state.last_check_time = esp_timer_get_time() / 1000000;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "║  TEST RUNNING                            ║");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "UI showing:");
    ESP_LOGI(TAG, "  • Real-time status");
    ESP_LOGI(TAG, "  • Inbox details");
    ESP_LOGI(TAG, "  • Recent operations");
    ESP_LOGI(TAG, "  • Message list (when received)");
    ESP_LOGI(TAG, "  • Statistics");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Checking for messages every %d seconds", 
             CONFIG_AGENTMAIL_TEST_CHECK_INTERVAL);
    ESP_LOGI(TAG, "Send email to: %s", inbox_id.c_str());
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Test running... (press RESET to exit)");
    ESP_LOGI(TAG, "");
    
    // Keep main task alive
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        ESP_LOGI(TAG, "Status: Sent=%d, Received=%d, Errors=%d, Checks=%d",
                 test_state.messages_sent,
                 test_state.messages_received,
                 test_state.errors,
                 test_state.check_count);
    }
}

