#ifndef AGENTMAIL_UI_TEST_H
#define AGENTMAIL_UI_TEST_H

/**
 * @file agentmail_ui_test.h
 * @brief LVGL-based AgentMail REST API test mode with visual feedback
 * 
 * Provides comprehensive testing of AgentMail functionality with
 * real-time visual feedback on the display. Shows inbox details,
 * operation status, and live message updates.
 * 
 * Features:
 * - Real-time status indicators
 * - Color-coded operation results
 * - Scrollable message list
 * - Live statistics dashboard
 * - Auto-refresh every 5 seconds
 * 
 * Falls back to console mode if display is unavailable.
 */
void start_agentmail_ui_test();

#endif // AGENTMAIL_UI_TEST_H

