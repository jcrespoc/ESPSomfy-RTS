#include <Arduino.h>
#include <PubSubClient.h>
#include <string.h>
#include "MQTTPurger.h"
#ifdef ELOG_SERIAL_LEVEL
#include "Logger.h"
#else

#define LOG_E_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define LOG_W_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define LOG_I_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define LOG_D_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define LOG_V_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

#define LOG_E_PRINTLN(str) Serial.println(str)
#define LOG_W_PRINTLN(str) Serial.println(str)
#define LOG_I_PRINTLN(str) Serial.println(str)
#define LOG_D_PRINTLN(str) Serial.println(str)
#define LOG_V_PRINTLN(str) Serial.println(str)

#define LOG_E_HEX(str, buff, len)
#define LOG_W_HEX(str, buff, len)
#define LOG_I_HEX(str, buff, len)
#define LOG_D_HEX(str, buff, len)
#define LOG_V_HEX(str, buff, len)

#endif // ELOG_SERIAL_LEVEL

MQTTPurgeManager mqttPurge;

namespace PurgeHelper {

  // MQTT wildcard validator (+ and #)
  bool topicMatches(const char* topic, const char* pattern) {
    const char* t = topic;
    const char* p = pattern;

    while (*t && *p) {
      if (*p == '#') {
        return true; // '#' at pattern end matches any remaining levels
      }
      if (*p == '+') {
        // '+' matches a single level (up to next '/')
        while (*t && *t != '/') t++;
        p++;
        continue;
      }
      if (*t != *p) {
        return false;
      }
      t++;
      p++;
    }

    if (*p == '#') return true;
    return (*t == '\0' && *p == '\0');
  }

  // Extracts the first purely numeric subtopic (e.g. /shades/5/..., /cover/.../5/config)
  int extractIdFromSubtopic(const char* topic) {
    if (!topic) return -1;

    const char* p = topic;
    while (*p) {
      while (*p == '/') p++;
      if (*p == '\0') break;

      const char* start = p;
      while (*p && *p != '/') p++;
      const char* end = p;

      bool isNumeric = true;
      for (const char* c = start; c < end; c++) {
        if (!isdigit((unsigned char)*c)) {
          isNumeric = false;
          break;
        }
      }

      if (isNumeric && start < end) {
        return atoi(start);
      }
    }
    return -1;
  }
}

void MQTTPurgeManager::addRule(const char* topicPattern, PurgeFilterFn shouldPurge, const char* description) {
  if (!topicPattern || !shouldPurge) return;
  PurgeRule rule;
  rule.topicPattern = topicPattern;
  rule.shouldPurge = shouldPurge;
  rule.description = description ? description : topicPattern;
  this->rules.push_back(rule);
}

void MQTTPurgeManager::clearRules() {
  this->rules.clear();
}

void MQTTPurgeManager::reset() {
  this->state = MQTTPurgeState::Idle;
  this->executedOnce = false;
  this->queue.clear();
  this->clearRules();
}

void MQTTPurgeManager::start() {
  if (this->executedOnce) return;
  this->executedOnce = true;
  this->queue.clear();
  this->state = MQTTPurgeState::Subscribing;  
}

void MQTTPurgeManager::subscribeAll(PubSubClient& client) {
  for (const auto& rule : this->rules) {
    LOG_D_PRINTF("[MQTT Purge] Subscribing to: %s", rule.topicPattern.c_str());
    client.subscribe(rule.topicPattern.c_str());
  }
}

void MQTTPurgeManager::unsubscribeAll(PubSubClient& client) {
  for (const auto& rule : this->rules) {
    LOG_D_PRINTF("[MQTT Purge] Unsubscribing from: %s", rule.topicPattern.c_str());
    client.unsubscribe(rule.topicPattern.c_str());
  }
}

void MQTTPurgeManager::onMessageReceived(const char* topic, const byte* payload, size_t length) {
  if (this->state != MQTTPurgeState::Collecting || length == 0) return;

  for (const auto& rule : this->rules) {
    if (PurgeHelper::topicMatches(topic, rule.topicPattern.c_str())) {
      LOG_V_PRINTF("[MQTT Purge] Pattern match: %s", rule.topicPattern.c_str());
      if (rule.shouldPurge && rule.shouldPurge(topic, payload, length)) {
        // Avoid duplicate entries in queue
        String t = String(topic);
        bool exists = false;
        for (const auto& item : this->queue) {
          if (item == t) { exists = true; break; }
        }
        if (!exists) {
          LOG_D_PRINTF("[MQTT Purge] Queued %s", topic);
          this->queue.push_back(t);
        }
      } else {
        LOG_D_PRINTF("[MQTT Purge] Preserved %s", topic);
      }
      break;
    }
  }
}

void MQTTPurgeManager::loop(PubSubClient& client) {
  if (!client.connected()) return;
  
  switch (this->state) {
    case MQTTPurgeState::Subscribing:
      this->subscribeAll(client);
      this->timer = millis() + 3000; // 3 seconds to collect retained messages
      this->state = MQTTPurgeState::Collecting;
      LOG_D_PRINTF("[MQTT Purge] Subscribed to purge rules. Waiting 3s...");
      break;

    case MQTTPurgeState::Collecting:
      if (millis() > this->timer) {
        this->state = MQTTPurgeState::Unsubscribing;
      }
      break;

    case MQTTPurgeState::Unsubscribing:
      this->unsubscribeAll(client);
      this->state = MQTTPurgeState::Draining;
      LOG_D_PRINTF("[MQTT Purge] Unsubscribed. %u obsolete retained messages queued to purge.", this->queue.size());
      break;

    case MQTTPurgeState::Draining:
      if (!this->queue.empty()) {
        String topicToPurge = this->queue.back();
        this->queue.pop_back();
        client.publish(topicToPurge.c_str(), (const uint8_t*)"", 0, true);
        LOG_I_PRINTF("[MQTT Purge] Purged: %s", topicToPurge.c_str());
      } else {
        this->state = MQTTPurgeState::Completed;
        LOG_D_PRINTF("[MQTT Purge] Reactive purge completed successfully.");
      }
      break;

    default:
      break;
  }
}