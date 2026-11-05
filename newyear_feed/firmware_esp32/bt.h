// ============================================================================
//  bt.h — Bluetooth LE «UART» (NUS-подобный сервис): с телефона/ПК можно
//  слать короткие команды: PLAY, PAUSE, NEXT, PREV, RELOAD, STATUS, BRIGHT:80
//  Работает параллельно с Wi-Fi.
// ============================================================================
#pragma once
#include <Arduino.h>
#include "BLEDevice.h"
#include "config.h"

// Стандартные UUID сервиса/характеристик ESP32 BLE UART примеров
#define NUS_SERVICE_UUID   "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // запись в ESP32
#define NUS_TX_CHAR_UUID   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // чтение из ESP32

class BTUart {
public:
    BLEServer   *server = nullptr;
    BLECharacteristic *rxChar = nullptr, *txChar = nullptr;
    bool connected = false;
    String lastCmd = "";
    std::function<void(String)> onCommand;

    void begin(std::function<void(String)> handler) {
        onCommand = handler;
        BLEDevice::init(BT_DEVICE_NAME);
        server = BLEDevice::createServer();

        BLEService *svc = server->createService(NUS_SERVICE_UUID);
        rxChar = svc->createCharacteristic(NUS_RX_CHAR_UUID,
                BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
        txChar = svc->createCharacteristic(NUS_TX_CHAR_UUID,
                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
        rxChar->setCallbacks(new RxCallbacks(this));
        svc->start();

        BLEAdvertising *adv = server->getAdvertising();
        adv->addServiceUUID(NUS_SERVICE_UUID);
        adv->setScanResponse(true);
        adv->start();
    }

    void notify(const String &s) {
        if (txChar && connected) txChar->setValue(s.c_str());
    }

    // колбэки пишутся отдельным классом, т.к. нужен доступ к this внешнего
    class RxCallbacks : public BLECharacteristicCallbacks {
        BTUart *owner;
    public:
        explicit RxCallbacks(BTUart *o) : owner(o) {}
        void onWrite(BLECharacteristic *c) override {
            String v = c->getValue();
            v.trim(); v.toUpperCase();
            if (v.length() && owner->onCommand) {
                owner->lastCmd = v;
                owner->onCommand(v);
            }
        }
    };
};

static BTUart bt;
