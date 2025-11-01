#ifndef AGENTMAIL_TEST_H
#define AGENTMAIL_TEST_H

/**
 * @file agentmail_test.h
 * @brief Console-based AgentMail REST API test mode
 * 
 * Provides comprehensive testing of AgentMail functionality with
 * detailed serial console output. Useful for debugging and devices
 * without displays.
 * 
 * Test sequence:
 * 1. Initialize WiFi and AgentMail client
 * 2. Create/get inbox
 * 3. Send test message
 * 4. Poll for new messages periodically
 * 5. Display detailed results in console
 */
void start_agentmail_test();

#endif // AGENTMAIL_TEST_H

