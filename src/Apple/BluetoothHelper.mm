// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#import "BluetoothHelper.hpp"
#import "BluetoothUuids.hpp"
#include "Device/Port/State.hpp"
#include "LogFile.hpp"
#import "NativeDetectDeviceListener.h"
#import "PortBridge.hpp"
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

static uint64_t
CurrentTimeMillis()
{
  return (uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0);
}

static const char *
PeripheralName(CBPeripheral *peripheral)
{
  return peripheral.name != nil ? peripheral.name.UTF8String : "<unnamed>";
}

static const char *
WriteTypeName(CBCharacteristicWriteType type)
{
  return type == CBCharacteristicWriteWithoutResponse ? "without_response"
                                                      : "with_response";
}

static BOOL
IsHm10WriteCharacteristic(CBCharacteristic *characteristic)
{
  if (characteristic == nil)
    return NO;

  static CBUUID *const hm10TxUuid = [CBUUID UUIDWithString:@"FFE2"];
  return [characteristic.UUID isEqual:hm10TxUuid];
}

static int kBTQueueKey;

static NSUInteger
QueueBytes(NSArray<NSData *> *queue, NSUInteger max_items)
{
  if (queue == nil || queue.count == 0)
    return 0;

  NSUInteger bytes = 0;
  NSUInteger i = 0;
  for (NSData *item in queue) {
    bytes += item.length;
    if (++i >= max_items)
      break;
  }

  return bytes;
}

static NSData *
DequeueChunk(NSMutableArray<NSData *> *queue, NSUInteger max_len)
{
  if (queue == nil || queue.count == 0)
    return nil;

  if (max_len == 0)
    max_len = NSUIntegerMax;

  NSMutableData *chunk = [NSMutableData data];
  while (queue.count > 0 && chunk.length < max_len) {
    NSData *item = queue.firstObject;
    const NSUInteger remaining = max_len - chunk.length;
    if (item.length <= remaining) {
      [chunk appendData:item];
      [queue removeObjectAtIndex:0];
    } else {
      [chunk appendBytes:item.bytes length:remaining];
      NSData *remainder =
          [item subdataWithRange:NSMakeRange(remaining, item.length - remaining)];
      queue[0] = remainder;
      break;
    }
  }

  if (chunk.length == 0)
    return nil;

  return chunk;
}

static NSUInteger
ChunkLimit(CBPeripheral *peripheral, CBCharacteristicWriteType type)
{
  static const NSUInteger kPreferredChunkSize = 20;
  NSUInteger max_len = [peripheral maximumWriteValueLengthForType:type];
  if (max_len == 0)
    max_len = kPreferredChunkSize;

  return MIN(max_len, kPreferredChunkSize);
}

@implementation IOSBluetoothManager {
  dispatch_queue_t _btQueue;
  dispatch_queue_t _rxQueue;
  NSMutableDictionary<CBPeripheral *, CBCharacteristic *> *_rxCharacteristics;
  NSMutableDictionary<CBPeripheral *, CBCharacteristic *> *_txCharacteristics;
  NSMutableDictionary<CBPeripheral *, NSMutableArray<NSData *> *> *_pendingWrites;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_writeInProgress;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_rxBytesSinceLog;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_rxNotifiesSinceLog;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_rxLastLogMs;
  NSMutableDictionary<CBPeripheral *, NSMutableData *> *_rxPending;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_rxScheduled;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_txBytesSinceLog;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_txWritesSinceLog;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_txLastLogMs;
  NSMutableDictionary<CBPeripheral *, NSNumber *> *_flushScheduled;
}

- (instancetype)init
{
  self = [super init];
  if (self) {
    dispatch_queue_attr_t attr =
        dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                                QOS_CLASS_USER_INTERACTIVE, 0);
    _btQueue = dispatch_queue_create("org.xcsoar.bluetooth", attr);
    _rxQueue = dispatch_queue_create("org.xcsoar.bluetooth.rx", attr);
    dispatch_queue_set_specific(_btQueue, &kBTQueueKey, &kBTQueueKey, nullptr);
    _centralManager = [[CBCentralManager alloc] initWithDelegate:self
                                                           queue:_btQueue];
    _discoveredPeripherals = [NSMutableDictionary dictionary];
    _listeners = [NSHashTable weakObjectsHashTable];
    _activeConnections = [NSMutableDictionary dictionary];
    _rxCharacteristics = [NSMutableDictionary dictionary];
    _txCharacteristics = [NSMutableDictionary dictionary];
    _pendingWrites = [NSMutableDictionary dictionary];
    _writeInProgress = [NSMutableDictionary dictionary];
    _rxBytesSinceLog = [NSMutableDictionary dictionary];
    _rxNotifiesSinceLog = [NSMutableDictionary dictionary];
    _rxLastLogMs = [NSMutableDictionary dictionary];
    _rxPending = [NSMutableDictionary dictionary];
    _rxScheduled = [NSMutableDictionary dictionary];
    _txBytesSinceLog = [NSMutableDictionary dictionary];
    _txWritesSinceLog = [NSMutableDictionary dictionary];
    _txLastLogMs = [NSMutableDictionary dictionary];
    _flushScheduled = [NSMutableDictionary dictionary];
  }
  return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
  switch (central.state) {
  case CBManagerStatePoweredOn:
    LogFormat("Bluetooth is ON");
    break;
  case CBManagerStatePoweredOff:
    LogFormat("Bluetooth is OFF");
    break;
  case CBManagerStateUnsupported:
    LogFormat("Bluetooth unsupported");
    break;
  case CBManagerStateUnauthorized:
    LogFormat("Bluetooth unauthorized");
    break;
  case CBManagerStateResetting:
    LogFormat("Bluetooth resetting");
    break;
  case CBManagerStateUnknown:
  default:
    LogFormat("Bluetooth state unknown");
    break;
  }
}

- (BOOL)isBluetoothEnabled
{
  return self.centralManager.state == CBManagerStatePoweredOn;
}

- (NSString *)nameForDeviceAddress:(NSString *)address
{
  __block NSString *name = nil;
  void (^block)(void) = ^{
    CBPeripheral *peripheral = self.discoveredPeripherals[address];
    if (peripheral != nil && peripheral.name.length > 0)
      name = peripheral.name;
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);

  return name;
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI
{
  bool peripheralWillBeDetected = true; // TODO (debug=true; prod=false)

  NSString *identifier = peripheral.identifier.UUIDString;
  self.discoveredPeripherals[identifier] = peripheral;

  // // Falls Pending-Connect → sofort verbinden
  // if (self.pendingConnectionAddress &&
  // 	[identifier isEqualToString:self.pendingConnectionAddress])
  // {
  // 	LogFormat("Pending device %s found, connecting...", [peripheral.name
  // UTF8String]); 	self.pendingConnectionAddress = nil; 	[self.centralManager
  // stopScan];

  // 	PortBridge *bridge = new PortBridge();
  // 	_activeConnections[peripheral] = [NSValue valueWithPointer:bridge];
  // 	peripheral.delegate = self;
  // 	[self.centralManager connectPeripheral:peripheral options:nil];
  // 	return;
  // }

  // TODO this does not work yet. For debugging set peripheralWillBeDetected = true
  NSArray<CBUUID *> *serviceUUIDs =
      advertisementData[CBAdvertisementDataServiceUUIDsKey];
  if (serviceUUIDs) {
    auto allServiceUuids = BluetoothUuids::getAllServiceUuids();
    for (CBUUID *uuid in serviceUUIDs) {
      NSString *uuidString = uuid.UUIDString;
      for (auto uuid_sv : allServiceUuids) {
        NSString *serviceUuidString =
            [NSString stringWithUTF8String:uuid_sv.data()];
        if ([serviceUuidString caseInsensitiveCompare:uuidString] ==
            NSOrderedSame) {
          peripheralWillBeDetected = true;
          LogFormat("===> DEBUG Service UUID found in advertisement: %s",
                    uuidString.UTF8String);
        }
      }
    }
  }

  if (peripheralWillBeDetected) {
    uint64_t features = 0;

    for (CBUUID *uuid in serviceUUIDs) {
      NSString *uuidString = uuid.UUIDString;

      NSString *hm10NSString =
          [NSString stringWithUTF8String:BluetoothUuids::HM10_SERVICE.data()];
      NSString *heartRateNSString = [NSString
          stringWithUTF8String:BluetoothUuids::HEART_RATE_SERVICE.data()];
      NSString *flytecNSString = [NSString
          stringWithUTF8String:BluetoothUuids::FLYTEC_SENSBOX_SERVICE.data()];

      if ([uuidString caseInsensitiveCompare:hm10NSString] == NSOrderedSame) {
        features |= DetectDeviceListener::FEATURE_HM10;
      } else if ([uuidString caseInsensitiveCompare:heartRateNSString] ==
                 NSOrderedSame) {
        features |= DetectDeviceListener::FEATURE_HEART_RATE;
      } else if ([uuidString caseInsensitiveCompare:flytecNSString] ==
                 NSOrderedSame) {
        features |= DetectDeviceListener::FEATURE_FLYTEC_SENSBOX;
      }
    }

    // LogFormat("=====> DEBUG FEATURES %llu", features);

    for (NativeDetectDeviceListener *listener in self.listeners) {
      // All devices detected via CoreBluetooth are iOS BLE devices. However,
      // BLUETOOTH_CLASSIC is used here because XCSoar requires it for the
      // interface and driver selection.
      int type =
          static_cast<int>(DetectDeviceListener::Type::BLUETOOTH_CLASSIC);
      [listener onDeviceDetected:type
                         address:identifier
                            name:[self nameForDeviceAddress:identifier]
                        features:features];
    }
  }
}

- (void)startScan
{
  void (^block)(void) = ^{
    [self.centralManager scanForPeripheralsWithServices:nil options:nil];
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);
}

- (void)stopScan
{
  void (^block)(void) = ^{
    [self.centralManager stopScan];
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);
}

- (void)addListener:(NativeDetectDeviceListener *)listener
{
  if (!listener)
    return;

  void (^block)(void) = ^{
    [self.listeners addObject:listener];
    [self.centralManager scanForPeripheralsWithServices:nil options:nil];
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);
}

- (void)removeListener:(NativeDetectDeviceListener *)listener
{
  if (!listener)
    return;

  void (^block)(void) = ^{
    [self.listeners removeObject:listener];
    [self.centralManager stopScan];
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);
}

- (void)connectSensor:(NSString *)deviceAddress
             listener:(SensorListener &)listener
{
  (void)listener;

  void (^block)(void) = ^{
    CBPeripheral *peripheral = self.discoveredPeripherals[deviceAddress];
    if (!peripheral) {
      LogFormat("Device %s not found", [deviceAddress UTF8String]);
      return;
    }

    LogFormat("Connecting to %s", [peripheral.name UTF8String]);
    peripheral.delegate = self;
    [self.centralManager connectPeripheral:peripheral options:nil];
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);
}

- (PortBridge *)connectToDevice:(NSString *)deviceAddress
{
  __block PortBridge *result = nullptr;
  void (^block)(void) = ^{
    CBPeripheral *peripheral = self.discoveredPeripherals[deviceAddress];

    if (!peripheral) {
      NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:deviceAddress];
      NSArray *peripherals =
          [self.centralManager retrievePeripheralsWithIdentifiers:@[ uuid ]];
      if (peripherals.count > 0) {
        peripheral = peripherals.firstObject;
        self.discoveredPeripherals[deviceAddress] = peripheral;
      }
    }

    if (!peripheral) {
      LogFormat("Device %s not found, scanning...", [deviceAddress UTF8String]);
      self.pendingConnectionAddress = deviceAddress;
      [self.centralManager scanForPeripheralsWithServices:nil options:nil];
      result = nullptr;
      return;
    }

    PortBridge *bridge = new PortBridge([deviceAddress UTF8String]);
    _activeConnections[peripheral] = [NSValue valueWithPointer:bridge];
    peripheral.delegate = self;

    // Do not create the PortBridge here yet, as the connection is asynchronous.
    [self.centralManager connectPeripheral:peripheral options:nil];
    result = bridge;
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);

  return result;
}

- (void)centralManager:(CBCentralManager *)central
    didConnectPeripheral:(CBPeripheral *)peripheral
{
  LogFormat("Connected with %s", [peripheral.name UTF8String]);
  [peripheral discoverServices:nil];
  //   PortBridge *bridge = new PortBridge();
  //   _activeConnections[peripheral] = [NSValue valueWithPointer:bridge];
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverServices:(NSError *)error
{
  if (error) {
    LogFormat("Error discovering services: %s", [[error localizedDescription] UTF8String]);
    return;
  }

  for (CBService *service in peripheral.services) {
    [peripheral discoverCharacteristics:nil forService:service];
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(NSError *)error
{
  if (error) {
    LogFormat("Error discovering characteristics: %s", [[error localizedDescription] UTF8String]);
    return;
  }

  CBCharacteristic *old_rx = _rxCharacteristics[peripheral];
  CBCharacteristic *rx = old_rx;
  CBCharacteristic *tx = _txCharacteristics[peripheral];
  CBUUID *hm10Uuid = [CBUUID UUIDWithString:@"FFE1"];
  CBCharacteristic *notify_candidate = nil;
  CBCharacteristic *indicate_candidate = nil;
  CBCharacteristic *write_candidate = nil;
  CBCharacteristic *dual_candidate = nil;
  CBCharacteristic *non_hm10_write_candidate = nil;
  CBCharacteristic *non_hm10_dual_candidate = nil;
  CBCharacteristic *non_hm10_indicate_candidate = nil;

  for (CBCharacteristic *characteristic in service.characteristics) {
    const CBCharacteristicProperties props = characteristic.properties;
    const BOOL isNotify = (props & CBCharacteristicPropertyNotify) != 0;
    const BOOL isIndicate = (props & CBCharacteristicPropertyIndicate) != 0;
    const BOOL isWrite =
        (props & CBCharacteristicPropertyWrite) ||
        (props & CBCharacteristicPropertyWriteWithoutResponse);
    const BOOL isHm10 = [characteristic.UUID isEqual:hm10Uuid];

    LogFormat("BT: Characteristic %s on %s (service %s) notify=%d indicate=%d write=%d wnr=%d",
              characteristic.UUID.UUIDString.UTF8String,
              PeripheralName(peripheral),
              service.UUID.UUIDString.UTF8String,
              isNotify ? 1 : 0,
              isIndicate ? 1 : 0,
              (props & CBCharacteristicPropertyWrite) ? 1 : 0,
              (props & CBCharacteristicPropertyWriteWithoutResponse) ? 1 : 0);

    if (isNotify && notify_candidate == nil)
      notify_candidate = characteristic;
    if (isIndicate && indicate_candidate == nil)
      indicate_candidate = characteristic;
    if (isWrite && write_candidate == nil)
      write_candidate = characteristic;
    if ((isNotify || isIndicate) && isWrite && dual_candidate == nil)
      dual_candidate = characteristic;

    if (isWrite && !isHm10 && non_hm10_write_candidate == nil)
      non_hm10_write_candidate = characteristic;
    if ((isNotify || isIndicate) && isWrite && !isHm10 &&
        non_hm10_dual_candidate == nil)
      non_hm10_dual_candidate = characteristic;
    if (isIndicate && !isHm10 && non_hm10_indicate_candidate == nil)
      non_hm10_indicate_candidate = characteristic;
  }

  if (non_hm10_indicate_candidate != nil) {
    rx = non_hm10_indicate_candidate;
  } else if (indicate_candidate != nil) {
    rx = indicate_candidate;
  } else if (non_hm10_dual_candidate != nil) {
    rx = non_hm10_dual_candidate;
    tx = non_hm10_dual_candidate;
  } else if (non_hm10_write_candidate != nil) {
    tx = non_hm10_write_candidate;
    if (notify_candidate != nil)
      rx = notify_candidate;
  } else if (dual_candidate != nil) {
    rx = dual_candidate;
    tx = dual_candidate;
  } else {
    if (notify_candidate != nil)
      rx = notify_candidate;
    if (write_candidate != nil)
      tx = write_candidate;
  }

  if (rx != nil && _rxCharacteristics[peripheral] != rx) {
    _rxCharacteristics[peripheral] = rx;
    LogFormat("BT: RX characteristic selected for %s: %s",
              PeripheralName(peripheral),
              rx.UUID.UUIDString.UTF8String);
  }

  if (tx != nil && _txCharacteristics[peripheral] != tx) {
    _txCharacteristics[peripheral] = tx;
    const CBCharacteristicProperties props = tx.properties;
    LogFormat("BT: TX characteristic selected for %s: %s (write=%d wnr=%d max_with=%lu max_wnr=%lu)",
              PeripheralName(peripheral),
              tx.UUID.UUIDString.UTF8String,
              (props & CBCharacteristicPropertyWrite) ? 1 : 0,
              (props & CBCharacteristicPropertyWriteWithoutResponse) ? 1 : 0,
              (unsigned long)[peripheral maximumWriteValueLengthForType:CBCharacteristicWriteWithResponse],
              (unsigned long)[peripheral maximumWriteValueLengthForType:CBCharacteristicWriteWithoutResponse]);
  }

  if (old_rx != nil && old_rx != rx && old_rx.isNotifying) {
    [peripheral setNotifyValue:NO forCharacteristic:old_rx];
    LogFormat("BT: Notifications disabled for %s (%s)",
              PeripheralName(peripheral),
              old_rx.UUID.UUIDString.UTF8String);
  }

  if (rx != nil) {
    const CBCharacteristicProperties props = rx.properties;
    const BOOL isNotify = (props & CBCharacteristicPropertyNotify) != 0;
    const BOOL isIndicate = (props & CBCharacteristicPropertyIndicate) != 0;
    if (isNotify || isIndicate) {
      if (!rx.isNotifying) {
        [peripheral setNotifyValue:YES forCharacteristic:rx];
        LogFormat("BT: Enabling notifications for %s (%s)",
                  PeripheralName(peripheral),
                  rx.UUID.UUIDString.UTF8String);
      }
    } else {
      LogFormat("BT: RX characteristic %s for %s is not notifiable",
                rx.UUID.UUIDString.UTF8String,
                PeripheralName(peripheral));
    }
  }

  if (tx != nil)
    [self flushPendingWritesForPeripheral:peripheral];
}

- (void)flushPendingWritesForPeripheral:(CBPeripheral *)peripheral
{
  CBCharacteristic *tx = _txCharacteristics[peripheral];
  if (tx == nil)
    return;

  NSMutableArray<NSData *> *queue = _pendingWrites[peripheral];
  if (queue == nil || queue.count == 0)
    return;

  if ([_writeInProgress[peripheral] boolValue])
    return;

  const CBCharacteristicProperties props = tx.properties;
  const BOOL supports_with_response =
      (props & CBCharacteristicPropertyWrite) != 0;
  const BOOL supports_without_response =
      (props & CBCharacteristicPropertyWriteWithoutResponse) != 0;

  if (!supports_with_response && !supports_without_response) {
    LogFormat("BT: TX characteristic %s for %s is not writable",
              tx.UUID.UUIDString.UTF8String, PeripheralName(peripheral));
    return;
  }

  const NSUInteger queued_bytes_hint = QueueBytes(queue, 64);
  const BOOL hm10_large_backlog =
      IsHm10WriteCharacteristic(tx) && queued_bytes_hint > 96;
  const BOOL use_without_response =
      supports_without_response &&
      (!supports_with_response || hm10_large_backlog);
  const CBCharacteristicWriteType type =
      use_without_response ? CBCharacteristicWriteWithoutResponse
                           : CBCharacteristicWriteWithResponse;
  const NSUInteger chunk_size = ChunkLimit(peripheral, type);

  if (use_without_response && !peripheral.canSendWriteWithoutResponse) {
    if (![_flushScheduled[peripheral] boolValue]) {
      _flushScheduled[peripheral] = @YES;
      const int64_t delay_ns = 2 * NSEC_PER_MSEC;
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay_ns), _btQueue, ^{
        _flushScheduled[peripheral] = @NO;
        [self flushPendingWritesForPeripheral:peripheral];
      });
    }
    return;
  }

  NSData *next = DequeueChunk(queue, chunk_size);
  if (next == nil)
    return;

  _writeInProgress[peripheral] = @YES;
  [peripheral writeValue:next forCharacteristic:tx type:type];

  const uint64_t now_ms = CurrentTimeMillis();
  const uint64_t last_ms =
      (uint64_t)[_txLastLogMs[peripheral] unsignedLongLongValue];
  const uint64_t bytes =
      (uint64_t)[_txBytesSinceLog[peripheral] unsignedLongLongValue] +
      (uint64_t)next.length;
  const uint64_t writes =
      (uint64_t)[_txWritesSinceLog[peripheral] unsignedLongLongValue] + 1;

  const NSUInteger queued_bytes = QueueBytes(queue, 64);
  if (last_ms == 0 || (now_ms - last_ms) >= 1000) {
    LogFormat("BT: TX writes bytes=%llu count=%llu chunk=%lu queued=%lu type=%s to %s (%s)",
              (unsigned long long)bytes,
              (unsigned long long)writes,
              (unsigned long)next.length,
              (unsigned long)queued_bytes,
              WriteTypeName(type),
              PeripheralName(peripheral),
              tx.UUID.UUIDString.UTF8String);
    _txLastLogMs[peripheral] = [NSNumber numberWithUnsignedLongLong:now_ms];
    _txBytesSinceLog[peripheral] = @0;
    _txWritesSinceLog[peripheral] = @0;
  } else {
    _txBytesSinceLog[peripheral] = [NSNumber numberWithUnsignedLongLong:bytes];
    _txWritesSinceLog[peripheral] = [NSNumber numberWithUnsignedLongLong:writes];
  }

  if (queue.count == 0) {
    [_pendingWrites removeObjectForKey:peripheral];
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(NSError *)error
{
  if (error != nil) {
    LogFormat("Error updating value for characteristic: %s", [error.localizedDescription UTF8String]);
    return;
  }

  NSValue *bridgeValue = _activeConnections[peripheral];
  if (bridgeValue == nil) {
    LogFormat("No active bridge for peripheral %s", [peripheral.name UTF8String]);
    return;
  }

  NSData *value = characteristic.value;
  if (value == nil || value.length == 0) {
    return;
  }

  const uint64_t nowMs = CurrentTimeMillis();
  const uint64_t lastMs =
      (uint64_t)[_rxLastLogMs[peripheral] unsignedLongLongValue];
  uint64_t rxBytesAccum =
      (uint64_t)[_rxBytesSinceLog[peripheral] unsignedLongLongValue] +
      (uint64_t)value.length;
  uint64_t rxNotifiesAccum =
      (uint64_t)[_rxNotifiesSinceLog[peripheral] unsignedLongLongValue] + 1;
  if (lastMs == 0 || (nowMs - lastMs) >= 1000) {
    LogFormat("BT: RX notify bytes=%llu count=%llu from %s (%s)",
              (unsigned long long)rxBytesAccum,
              (unsigned long long)rxNotifiesAccum,
              PeripheralName(peripheral),
              characteristic.UUID.UUIDString.UTF8String);
    _rxLastLogMs[peripheral] = [NSNumber numberWithUnsignedLongLong:nowMs];
    _rxBytesSinceLog[peripheral] = @0;
    _rxNotifiesSinceLog[peripheral] = @0;
  } else {
    _rxBytesSinceLog[peripheral] =
        [NSNumber numberWithUnsignedLongLong:rxBytesAccum];
    _rxNotifiesSinceLog[peripheral] =
        [NSNumber numberWithUnsignedLongLong:rxNotifiesAccum];
  }

  CBCharacteristic *rxCharacteristic = _rxCharacteristics[peripheral];
  if (rxCharacteristic != nil && rxCharacteristic != characteristic) {
    LogFormat("BT: RX data from non-selected characteristic %s (selected=%s, len=%lu) on %s",
              characteristic.UUID.UUIDString.UTF8String,
              rxCharacteristic.UUID.UUIDString.UTF8String,
              (unsigned long)value.length,
              PeripheralName(peripheral));
  }

  NSMutableData *pending = _rxPending[peripheral];
  if (pending == nil) {
    pending = [NSMutableData data];
    _rxPending[peripheral] = pending;
  }
  [pending appendData:value];

  if (![_rxScheduled[peripheral] boolValue]) {
    _rxScheduled[peripheral] = @YES;
    const int64_t delay_ns = 1 * NSEC_PER_MSEC;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay_ns), _btQueue, ^{
      NSMutableData *buffer = _rxPending[peripheral];
      if (buffer == nil || buffer.length == 0) {
        _rxScheduled[peripheral] = @NO;
        return;
      }

      NSData *payload = [buffer copy];
      [buffer setLength:0];
      _rxScheduled[peripheral] = @NO;

      dispatch_async(_rxQueue, ^{
        NSValue *current_bridge = _activeConnections[peripheral];
        if (current_bridge == nil)
          return;

        auto *bridge = reinterpret_cast<PortBridge *>([current_bridge pointerValue]);
        DataHandler *listener = bridge->getInputListener();
        if (listener == nullptr) {
          LogFormat("BT: RX dropped because input listener is not set for %s",
                    PeripheralName(peripheral));
          return;
        }

        listener->DataReceived({(const std::byte *)payload.bytes,
                                (size_t)payload.length});
      });
    });
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didWriteValueForCharacteristic:(CBCharacteristic *)characteristic
                             error:(NSError *)error
{
  if (error != nil) {
    LogFormat("BT: TX write failed for %s (%s): %s",
              PeripheralName(peripheral),
              characteristic.UUID.UUIDString.UTF8String,
              error.localizedDescription.UTF8String);
  }

  _writeInProgress[peripheral] = @NO;
  [self flushPendingWritesForPeripheral:peripheral];
}

- (void)peripheralIsReadyToSendWriteWithoutResponse:(CBPeripheral *)peripheral
{
  _writeInProgress[peripheral] = @NO;
  [self flushPendingWritesForPeripheral:peripheral];
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic
                                          error:(NSError *)error
{
  if (error != nil) {
    LogFormat("BT: Notify state update failed for %s (%s): %s",
              PeripheralName(peripheral),
              characteristic.UUID.UUIDString.UTF8String,
              error.localizedDescription.UTF8String);
    return;
  }

  LogFormat("BT: Notifications %s for %s (%s)",
            characteristic.isNotifying ? "enabled" : "disabled",
            PeripheralName(peripheral),
            characteristic.UUID.UUIDString.UTF8String);
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(NSError *)error
{
  LogFormat("Connection to %s failed: %s", [peripheral.name UTF8String],
            [error.localizedDescription UTF8String]);

  [_activeConnections removeObjectForKey:peripheral];
  [_rxCharacteristics removeObjectForKey:peripheral];
  [_txCharacteristics removeObjectForKey:peripheral];
  [_pendingWrites removeObjectForKey:peripheral];
  [_writeInProgress removeObjectForKey:peripheral];
  [_rxBytesSinceLog removeObjectForKey:peripheral];
  [_rxNotifiesSinceLog removeObjectForKey:peripheral];
  [_rxLastLogMs removeObjectForKey:peripheral];
  [_rxPending removeObjectForKey:peripheral];
  [_rxScheduled removeObjectForKey:peripheral];
  [_txBytesSinceLog removeObjectForKey:peripheral];
  [_txWritesSinceLog removeObjectForKey:peripheral];
  [_txLastLogMs removeObjectForKey:peripheral];
  [_flushScheduled removeObjectForKey:peripheral];
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(NSError *)error
{
  if (error != nil) {
    LogFormat("Disconnected from %s: %s",
              PeripheralName(peripheral),
              error.localizedDescription.UTF8String);
  } else {
    LogFormat("Disconnected from %s", PeripheralName(peripheral));
  }

  [_activeConnections removeObjectForKey:peripheral];
  [_rxCharacteristics removeObjectForKey:peripheral];
  [_txCharacteristics removeObjectForKey:peripheral];
  [_pendingWrites removeObjectForKey:peripheral];
  [_writeInProgress removeObjectForKey:peripheral];
  [_rxBytesSinceLog removeObjectForKey:peripheral];
  [_rxNotifiesSinceLog removeObjectForKey:peripheral];
  [_rxLastLogMs removeObjectForKey:peripheral];
  [_rxPending removeObjectForKey:peripheral];
  [_rxScheduled removeObjectForKey:peripheral];
  [_txBytesSinceLog removeObjectForKey:peripheral];
  [_txWritesSinceLog removeObjectForKey:peripheral];
  [_txLastLogMs removeObjectForKey:peripheral];
  [_flushScheduled removeObjectForKey:peripheral];
}

- (BOOL)writeData:(NSData *)data toDeviceAddress:(NSString *)address
{
  if (data == nil || data.length == 0)
    return YES;

  NSData *data_copy = [data copy];
  NSString *addr_copy = [address copy];
  __block BOOL queued = NO;

  void (^block)(void) = ^{
    CBPeripheral *peripheral = self.discoveredPeripherals[addr_copy];
    if (!peripheral) {
      LogFormat("Peripheral %s not found", addr_copy.UTF8String);
      return;
    }

    NSMutableArray<NSData *> *queue = _pendingWrites[peripheral];
    if (queue == nil) {
      queue = [NSMutableArray array];
      _pendingWrites[peripheral] = queue;
    }

    if (queue.count >= 128) {
      LogFormat("BT: Pending write queue overflow for %s, dropping %lu items",
                PeripheralName(peripheral), (unsigned long)queue.count);
      [queue removeAllObjects];
    }

    [queue addObject:data_copy];
    queued = YES;

    CBCharacteristic *tx = _txCharacteristics[peripheral];
    if (tx == nil && (peripheral.services == nil || peripheral.services.count == 0)) {
      LogFormat("BT: Queued TX write len=%lu for %s (no TX characteristic yet)",
                (unsigned long)data_copy.length, PeripheralName(peripheral));
      return;
    }

    if (tx == nil) {
      for (CBService *service in peripheral.services) {
        for (CBCharacteristic *characteristic in service.characteristics) {
          if (characteristic.properties & CBCharacteristicPropertyWrite ||
              characteristic.properties & CBCharacteristicPropertyWriteWithoutResponse) {
            _txCharacteristics[peripheral] = characteristic;
            tx = characteristic;
            LogFormat("BT: TX characteristic selected in writeData for %s: %s",
                      PeripheralName(peripheral),
                      characteristic.UUID.UUIDString.UTF8String);
            break;
          }
        }
        if (tx != nil)
          break;
      }
    }

    if (tx == nil) {
      LogFormat("BT: Queued TX write len=%lu for %s (no writable characteristic yet)",
                (unsigned long)data_copy.length, PeripheralName(peripheral));
      return;
    }

    if (data_copy.length > 20) {
      const CBCharacteristicProperties props = tx.properties;
      const BOOL can_write_with_response =
          (props & CBCharacteristicPropertyWrite) != 0;
      const CBCharacteristicWriteType type =
          can_write_with_response ? CBCharacteristicWriteWithResponse
                                  : CBCharacteristicWriteWithoutResponse;
      const NSUInteger chunk_size = ChunkLimit(peripheral, type);
      const NSUInteger chunks =
          (data_copy.length + chunk_size - 1) / chunk_size;
      LogFormat("BT: TX start len=%lu chunks=%lu chunk_size=%lu type=%s char=%s on %s",
                (unsigned long)data_copy.length,
                (unsigned long)chunks,
                (unsigned long)chunk_size,
                WriteTypeName(type),
                tx.UUID.UUIDString.UTF8String,
                PeripheralName(peripheral));
    }

    if (![_flushScheduled[peripheral] boolValue]) {
      _flushScheduled[peripheral] = @YES;
      const int64_t delay_ns = 1 * NSEC_PER_MSEC;
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay_ns), _btQueue, ^{
        _flushScheduled[peripheral] = @NO;
        [self flushPendingWritesForPeripheral:peripheral];
      });
    }
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr) {
    block();
  } else {
    dispatch_sync(_btQueue, block);
  }

  return queued;
}

- (int)portStateForDeviceAddress:(NSString *)address
{
  if (address == nil || address.length == 0)
    return static_cast<int>(PortState::FAILED);

  __block int state = static_cast<int>(PortState::FAILED);
  void (^block)(void) = ^{
    CBPeripheral *peripheral = self.discoveredPeripherals[address];
    if (peripheral == nil) {
      if (self.pendingConnectionAddress != nil &&
          [self.pendingConnectionAddress isEqualToString:address]) {
        state = static_cast<int>(PortState::LIMBO);
      }
      return;
    }

    NSValue *bridge = _activeConnections[peripheral];
    CBCharacteristic *tx = _txCharacteristics[peripheral];
    switch (peripheral.state) {
    case CBPeripheralStateConnected:
      if (bridge == nil) {
        state = static_cast<int>(PortState::FAILED);
      } else if (tx != nil || peripheral.services != nil) {
        // Treat a connected BLE peripheral as READY. Characteristic discovery
        // may still be in progress, but failing early causes spurious closes.
        state = static_cast<int>(PortState::READY);
      } else {
        state = static_cast<int>(PortState::LIMBO);
      }
      break;

    case CBPeripheralStateConnecting:
    case CBPeripheralStateDisconnecting:
      state = static_cast<int>(PortState::LIMBO);
      break;

    case CBPeripheralStateDisconnected:
    default:
      state = (bridge != nil)
          ? static_cast<int>(PortState::LIMBO)
          : static_cast<int>(PortState::FAILED);
      break;
    }
  };

  if (dispatch_get_specific(&kBTQueueKey) != nullptr)
    block();
  else
    dispatch_sync(_btQueue, block);

  return state;
}

- (BOOL)drainWritesForDeviceAddress:(NSString *)address
                          timeoutMs:(NSUInteger)timeoutMs
{
  if (address == nil || address.length == 0)
    return NO;

  if (timeoutMs == 0)
    timeoutMs = 2000;

  const uint64_t deadlineMs = CurrentTimeMillis() + (uint64_t)timeoutMs;
  while (true) {
    __block BOOL peripheralMissing = NO;
    __block BOOL drained = NO;

    void (^block)(void) = ^{
      CBPeripheral *peripheral = self.discoveredPeripherals[address];
      if (peripheral == nil) {
        peripheralMissing = YES;
        return;
      }

      if (peripheral.state != CBPeripheralStateConnected) {
        drained = NO;
        return;
      }

      NSMutableArray<NSData *> *queue = _pendingWrites[peripheral];
      const BOOL hasQueued = queue != nil && queue.count > 0;
      const BOOL inFlight = [_writeInProgress[peripheral] boolValue];
      drained = !hasQueued && !inFlight;
      if (!drained && _txCharacteristics[peripheral] != nil)
        [self flushPendingWritesForPeripheral:peripheral];
    };

    if (dispatch_get_specific(&kBTQueueKey) != nullptr) {
      block();
      return drained && !peripheralMissing;
    }

    dispatch_sync(_btQueue, block);

    if (peripheralMissing)
      return NO;
    if (drained)
      return YES;
    if (CurrentTimeMillis() >= deadlineMs)
      return NO;

    [NSThread sleepForTimeInterval:0.005];
  }
}

@end

BluetoothHelperIOS::BluetoothHelperIOS()
{
  manager = [[IOSBluetoothManager alloc] init];
}

BluetoothHelperIOS::~BluetoothHelperIOS() { manager = nil; }

bool
BluetoothHelperIOS::HasBluetoothSupport() const noexcept
{
  CBManagerState state = manager.centralManager.state;
  if (state == CBManagerStateUnsupported) {
    return false;
  }
  return true;
}

bool
BluetoothHelperIOS::IsEnabled() const noexcept
{
  return [manager isBluetoothEnabled];
}

const char *
BluetoothHelperIOS::GetNameFromAddress(const char *address) const noexcept
{
  if (!address) return nullptr;

  NSString *addrStr = [NSString stringWithUTF8String:address];
  NSString *name = [manager nameForDeviceAddress:addrStr];

  if (!name) return nullptr;

  // TODO: Must return a pointer to static memory
  // Using a simple static buffer here (not thread-safe, for demo purposes)
  static thread_local char buffer[256];
  strncpy(buffer, [name UTF8String], sizeof(buffer));
  buffer[sizeof(buffer) - 1] = '\0';
  return buffer;
}

NativeDetectDeviceListener *
BluetoothHelperIOS::AddDetectDeviceListener(
    DetectDeviceListener &listener) noexcept
{
  NativeDetectDeviceListener *nativeListener =
      [[NativeDetectDeviceListener alloc] initWithCppListener:&listener];
  [manager addListener:nativeListener];
  return nativeListener;
}

void
BluetoothHelperIOS::RemoveDetectDeviceListener(
    NativeDetectDeviceListener *listener) noexcept
{
  [manager removeListener:listener];
}

void
BluetoothHelperIOS::connectSensor(const char *address,
                                  SensorListener &listener)
{
  if (!address) return;

  NSString *addrStr = [NSString stringWithUTF8String:address];
  [manager connectSensor:addrStr listener:listener];
}

PortBridge *
BluetoothHelperIOS::connect(const char *address)
{
  if (!address) return nullptr;

  NSString *addrStr = [NSString stringWithUTF8String:address];
  return [manager connectToDevice:addrStr];
}

PortBridge *
BluetoothHelperIOS::connectHM10(const char *address)
{
  if (!address) return nullptr;

  NSString *addrStr = [NSString stringWithUTF8String:address];
  return [manager connectToDevice:addrStr];
}

PortBridge *
BluetoothHelperIOS::createServer()
{
  // TODO
  return nullptr;
  // PortBridge *bridge = [manager createBluetoothServer];
  // return bridge;
}

extern "C" BluetoothHelper *
CreateBluetoothHelper()
{
  return new BluetoothHelperIOS();
}
