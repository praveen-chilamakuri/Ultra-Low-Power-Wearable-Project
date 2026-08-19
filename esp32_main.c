// This is the ArduinoIDE code for ESP32, which always stays in deep-sleep mode unless there is an alert and woke-up by STM32.
// ESP32, then enables & advertises the Alert through BLE.
// Updated: 06/08/2026
// Github: https://github.com/praveen-chilamakuri/Ultra-Low-Power-Wearable-Project


#include <esp_sleep.h>           // Deep/light sleep APIs
#include <esp_bt.h>              // Bluetooth controller memory management
#include <BLEDevice.h>           // BLE stack
#include <BLEAdvertising.h>      // BLE advertising control

// GPIO pin used by STM32 to wake the ESP32.
// Must be an RTC-capable pin for EXT0 wakeup.
#define WAKE_PIN GPIO_NUM_4  

void setup()
{
    Serial.begin(115200);
    delay(100);     // Allow serial to stabilize

    // ------------------------------------------------------------
    // 1. Determine why the ESP32 woke up
    // ------------------------------------------------------------
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    if (cause == ESP_SLEEP_WAKEUP_EXT0)
    {

        // ------------------------------------------------------------
        // CASE A: STM32 triggered wakeup (alert condition)
        // ------------------------------------------------------------
        Serial.println("Wake: Alert trigger received.");

        // Disable Classic Bluetooth to save power
        // We only need BLE for advertising.
        esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

        // Prepare BLE controller configuration
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

        // Initialize and enable BLE controller
        esp_bt_controller_init(&bt_cfg);
        esp_bt_controller_enable(ESP_BT_MODE_BLE);

        // Initialize BLE stack
        BLEDevice::init("Alert");
        
        // Get advertising object
        BLEAdvertising *adv = BLEDevice::getAdvertising();

        // Disable scan response + preferred connection parameters
        adv->setScanResponse(false);
        adv->setMinPreferred(0x00);
        adv->setMaxPreferred(0x00);

        // ------------------------------------------------------------
        // 2. Build BLE advertisement packet
        // ------------------------------------------------------------
        BLEAdvertisementData advData;

        // Flags: general discoverable, no BR/EDR (BLE only)
        advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

        // Device name shown in BLE scanners
        advData.setName("Alert");

        // Manufacturer data 
        advData.setManufacturerData("Alert");

        // Apply advertisement data
        adv->setAdvertisementData(advData);

        // Start BLE advertising
        adv->start();
        Serial.println("BLE Alert advertising started.");

        // ------------------------------------------------------------
        // 3. Enter light sleep for 1.5 seconds
        //    BLE hardware continues advertising while CPU sleeps.
        // ------------------------------------------------------------
        esp_sleep_enable_timer_wakeup(1500000);   // microseconds

        Serial.println("Entering light sleep during advertising window...");
        Serial.flush();    // Ensure serial messages print before sleeping

        // CPU sleeps, BLE hardware continues broadcasting
        esp_light_sleep_start();

        Serial.println("Alert window complete. Sleeping again.");
    }
    else
    {
        // ------------------------------------------------------------
        // CASE B: Cold boot (power-on or reset)
        // ------------------------------------------------------------
        Serial.println("Cold boot. Going directly to deep sleep.");
    }

    // ------------------------------------------------------------
    // 4. Configure EXT0 wakeup
    //    STM32 drives WAKE_PIN HIGH to wake ESP32.
    // ------------------------------------------------------------
    esp_sleep_enable_ext0_wakeup(WAKE_PIN, 1);     // Wake on HIGH level

    Serial.println("Entering deep sleep.");
    delay(50);

    // ------------------------------------------------------------
    // 5. Enter deep sleep (lowest power mode)
    // ------------------------------------------------------------
    esp_deep_sleep_start();
}

void loop() 
{
       // Empty — ESP32 never runs loop() because it always sleeps.             
}
