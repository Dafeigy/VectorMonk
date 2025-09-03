/*
  An example showing rainbow colours on a 1.8" TFT LCD screen
  and to show a basic example of font use.

  Make sure all the display driver and pin connections are correct by
  editing the User_Setup.h file in the TFT_eSPI library folder.

  Note that yield() or delay(0) must be called in long duration for/while
  loops to stop the ESP8266 watchdog triggering.

  #########################################################################
  ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
  #########################################################################
*/
#include <Arduino.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include "pin_config.h"
#include <Wire.h>
#include "SensorQMI8658.hpp"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// 定义服务UUID（可以自定义）
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// 定义特征UUID（可以自定义）
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer *pServer = NULL;
BLEAdvertising *pAdvertising = NULL;
BLECharacteristic* pCharacteristic = NULL;
// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
// #error  "The current version is not supported for the time being, please use a version below Arduino ESP32 3.0"
// #endif
#define USE_WIRE

#ifndef SENSOR_SDA
#define SENSOR_SDA 18
#endif

#ifndef SENSOR_SCL
#define SENSOR_SCL 17
#endif

#ifndef SENSOR_IRQ
#define SENSOR_IRQ -1
#endif
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
#define IMU_CS 5

SensorQMI8658 qmi;

IMUdata acc;
IMUdata gyr;
unsigned long targetTime = 0;
byte red = 31;
byte green = 0;
byte blue = 0;
byte state = 0;
unsigned int colour = red << 11;
uint32_t runing = 0;

void setup(void)
{
    Serial.begin(115200);

#ifdef USE_WIRE
    // Using WIRE !!
    if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, SENSOR_SDA, SENSOR_SCL))
    {
        Serial.println("Failed to find QMI8658 - check your wiring!");
        while (1)
        {
            delay(1000);
        }
    }
#else
    if (!qmi.begin(IMU_CS))
    {
        Serial.println("Failed to find QMI8658 - check your wiring!");
        while (1)
        {
            delay(1000);
        }
    }
#endif

    Serial.print("Device ID:");
    Serial.println(qmi.getChipID(), HEX);
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    tft.init();
    tft.setCursor(0, 0, 2);
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.print("Device ID:");
    tft.println(qmi.getChipID(), HEX);
    qmi.configAccelerometer(

        SensorQMI8658::ACC_RANGE_4G,

        SensorQMI8658::ACC_ODR_1000Hz,

        SensorQMI8658::LPF_MODE_0);

    qmi.configGyroscope(

        SensorQMI8658::GYR_RANGE_64DPS,

        SensorQMI8658::GYR_ODR_896_8Hz,

        SensorQMI8658::LPF_MODE_3);

    // In 6DOF mode (accelerometer and gyroscope are both enabled),
    // the output data rate is derived from the nature frequency of gyroscope
    qmi.enableGyroscope();
    qmi.enableAccelerometer();

    // Print register configuration information
    qmi.dumpCtrlRegister();

    Serial.println("Initializing...");
    delay(500);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(12, 4, 4);
    // tft.fillRect(12, 0, 300, 28, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.println("[NUL4i@ESP32S3]");
    tft.setCursor(12, 30, 2);
    tft.setTextColor(TFT_WHITE);
    tft.println("DEVICE NAME: [VectorMonk]");
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(12, 60, 2);
    tft.println("[ACCL]");
    tft.setCursor(12, 120, 2);
    tft.println("[GYRO]");
    BLEDevice::init("[NUL4i]");

    // 创建BLE服务器
    pServer = BLEDevice::createServer();

    // 创建服务
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // 创建特征
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE);
    // 启动服务
    
    pService->start();

    // 获取广播对象
    pAdvertising = pServer->getAdvertising();
    BLEDevice::startAdvertising();
}

void loop()
{
    pAdvertising->start();

    if (qmi.getDataReady())
    {
        char buffer[100];

        if (qmi.getAccelerometer(acc.x, acc.y, acc.z))
        {
            tft.setTextColor(TFT_DARKCYAN);
            tft.fillRect(12, 80, 140, 20, TFT_BLACK);
            tft.setCursor(12, 80, 2);
            tft.print(acc.x);
            tft.print(",");
            tft.print(acc.y);
            tft.print(",");
            tft.print(acc.z);
        }

        if (qmi.getGyroscope(gyr.x, gyr.y, gyr.z))
        {

            tft.fillRect(12, 140, 140, 20, TFT_BLACK);
            tft.setCursor(12, 140, 2);
            tft.print(gyr.x);
            tft.print(",");
            tft.print(gyr.y);
            tft.print(",");
            tft.print(gyr.z);
        }
        Serial.printf("\t\t\t\t > %lu  %.2f *C\n", qmi.getTimestamp(), qmi.getTemperature_C());
        ;
        sprintf(buffer, "[Accl]\n%.2f,%.2f,%.2f\n\n[GYRO]\n%.2f,%.2f,%.2f", acc.x, acc.y, acc.z, gyr.x,gyr.y,gyr.z);
        pCharacteristic->setValue(String(buffer).c_str());
        
        // 通知已连接的客户端（如果有客户端订阅了通知）
        pCharacteristic->notify();
    }
    delay(30);
}
