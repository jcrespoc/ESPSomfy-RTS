#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
//#include <esp_task_wdt.h>
#include "ConfigSettings.h"
#include "Network.h"
#include "Web.h"
#include "Sockets.h"
#include "Utils.h"
#include "Somfy.h"
#include "MQTT.h"
#include "GitOTA.h"
#include "Recovery.h"

ConfigSettings settings;
Web webServer;
SocketEmitter sockEmit;
Network net;
rebootDelay_t rebootDelay;
SomfyShadeController somfy;
MQTTClass mqtt;
GitUpdater git;

TaskHandle_t networkTaskHandle = nullptr;
TaskHandle_t somfyTaskHandle = nullptr;
TaskHandle_t gitTaskHandle = nullptr;


void networkLoopTask(void *pvParameters) {
  net.setup();

  while(1) {
    net.loop();

    if(net.connected() || net.softAPOpened) {
      webServer.loop();     
      sockEmit.loop();
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void somfyLoopTask(void *pvParameters) {
  somfy.begin();

  while(1) {
    somfy.loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void gitLoopTask(void *pvParameters) {
  // Dedicated low-priority task for GitHub updates (can block without affecting network)
  const TickType_t xDelay = pdMS_TO_TICKS(10000);
  
  while(1) {
    // Only attempt git updates when network is stable and not rebooting
    if(!rebootDelay.reboot && net.connected() && !net.softAPOpened) {
      git.loop();
    }
    vTaskDelay(xDelay);  // Longer delay for low-priority task
  }
}

uint32_t oldheap = 0;
void setup() {
  #if defined(LED_PIN) && LED_PIN != -1
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  #endif
  Serial.begin(115200);
  Serial.println();
  Serial.println("Startup/Boot....");
  handlePowerCycleReset();
  Serial.println("Mounting File System...");
  if(LittleFS.begin()) Serial.println("File system mounted successfully");
  else Serial.println("Error mounting file system");
  if(_pendingFactory) performFactoryReset();
  settings.begin();
  if(_pendingNetSecuRecovery) resetAccessAndNetworkConfig();
  if(WiFi.status() == WL_CONNECTED) WiFi.disconnect(true);
  delay(10);
  Serial.println();
  webServer.startup();
  webServer.begin();
  delay(1000);
 
    //net.setup();
  xTaskCreate (
    networkLoopTask,                     // Task function
    "NetworkTask",                       // Task name
    8192,                                // Stack size (bytes)
    NULL,                                // Task parameter
    2,                                   // Priority (lower than main loop)
    &networkTaskHandle                  // Task handle
  );

  //somfy.begin();
  xTaskCreate(
    somfyLoopTask,                       // Task function
    "SomfyTask",                         // Task name
    4096,                                // Stack size (bytes)
    NULL,                                // Task parameter
    2,                                   // Priority (lowest - background)
    &somfyTaskHandle                    // Task handle
  );

  xTaskCreate(
    gitLoopTask,                         // Task function
    "GitTask",                           // Task name
    8192,                                // Stack size (bytes)
    NULL,                                // Task parameter
    1,                                   // Priority (lowest - HTTP updates can block)
    &gitTaskHandle                       // Task handle
  );

  //esp_task_wdt_init(15, true); //enable panic so ESP32 restarts
  //esp_task_wdt_add(NULL); //add current thread to WDT watch

}

void loop() {
  // put your main code here, to run repeatedly:
  //uint32_t heap = ESP.getFreeHeap();
  if(rebootDelay.reboot && millis() > rebootDelay.rebootTime) {
    Serial.print("Rebooting after ");
    Serial.print(rebootDelay.rebootTime);
    Serial.println("ms");
    net.end();
    ESP.restart();
    return;
  }
  /*
  uint32_t timing = millis();

  net.loop();
  if(millis() - timing > 100) Serial.printf("Timing Net: %ldms\n", millis() - timing);
  timing = millis();
  //esp_task_wdt_reset();
  somfy.loop();
  if(millis() - timing > 100) Serial.printf("Timing Somfy: %ldms\n", millis() - timing);
  timing = millis();  

  //esp_task_wdt_reset();
  if(net.connected() || net.softAPOpened) {
    if(!rebootDelay.reboot && net.connected() && !net.softAPOpened) {
      git.loop();
      //esp_task_wdt_reset();
    }
    webServer.loop();
    //esp_task_wdt_reset();
    if(millis() - timing > 100) Serial.printf("Timing WebServer: %ldms\n", millis() - timing);
    //esp_task_wdt_reset();
    timing = millis();
    sockEmit.loop();
    if(millis() - timing > 100) Serial.printf("Timing Socket: %ldms\n", millis() - timing);
    //esp_task_wdt_reset();
    timing = millis();
  }
  */
  if(rebootDelay.reboot && millis() > rebootDelay.rebootTime) {
    net.end();
    ESP.restart();
  }
  //esp_task_wdt_reset();
}
