// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef __APPLE__

#include <TargetConditionals.h>
#include "LogFile.hpp"
#include "Services.hpp"
#include "BluetoothHelper.hpp"
#import <AVFoundation/AVFoundation.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

#include <cassert>

BluetoothHelper *bluetooth_helper;

// Initialize apple services - this will be called from the main XCSoar startup
void
InitializeAppleServices()
{
#if TARGET_OS_IPHONE
  // Setup AVAudioSession for better audio playback
  NSError *error = nil;
  AVAudioSession *session = [AVAudioSession sharedInstance];
  [session setCategory:AVAudioSessionCategoryPlayback
           withOptions:AVAudioSessionCategoryOptionMixWithOthers
                 error:&error];
  [session setActive:YES error:&error];
  if (error) {
    LogFormat("AVAudioSession initialize error: %s", [[error localizedDescription] UTF8String]);
  }

  // Setup bluetooth helper
  bluetooth_helper = new BluetoothHelper();
#endif
}

// Cleanup apple services - this will be called from XCSoar shutdown
void
DeinitializeAppleServices()
{
#if TARGET_OS_IPHONE
  // Deinitialize AVAudioSession
  NSError *error = nil;
  [[AVAudioSession sharedInstance] setActive:NO error:&error];
  if (error) {
    LogFormat("AVAudioSession deinitialize error: %s", [[error localizedDescription] UTF8String]);
  }

  // Deinitialize bluetooth helper
  if (bluetooth_helper) {
    delete bluetooth_helper;
    bluetooth_helper = nullptr;
  }
#endif
}

#if TARGET_OS_IPHONE

/** nesting depth of Begin/EndAppleBulkTransfer() */
static unsigned bulk_transfer_depth;

static UIBackgroundTaskIdentifier bulk_transfer_task =
  UIBackgroundTaskInvalid;

#endif

void
BeginAppleBulkTransfer() noexcept
{
#if TARGET_OS_IPHONE
  if (bulk_transfer_depth++ > 0)
    return;

  UIApplication *application = [UIApplication sharedApplication];

  /* prevent auto-lock: when the phone locks, iOS suspends the app
     and all Bluetooth traffic stalls, aborting the transfer */
  application.idleTimerDisabled = YES;

  /* additionally request background execution time so the transfer
     survives a brief manual lock or app switch */
  bulk_transfer_task = [application
    beginBackgroundTaskWithName:@"BulkTransfer"
              expirationHandler:^{
      /* the system grace period is over; the task must be ended, and
         the app will be suspended until it returns to the
         foreground */
      if (bulk_transfer_task != UIBackgroundTaskInvalid) {
        [[UIApplication sharedApplication]
          endBackgroundTask:bulk_transfer_task];
        bulk_transfer_task = UIBackgroundTaskInvalid;
      }
    }];
#endif
}

void
EndAppleBulkTransfer() noexcept
{
#if TARGET_OS_IPHONE
  assert(bulk_transfer_depth > 0);

  if (--bulk_transfer_depth > 0)
    return;

  UIApplication *application = [UIApplication sharedApplication];

  application.idleTimerDisabled = NO;

  if (bulk_transfer_task != UIBackgroundTaskInvalid) {
    [application endBackgroundTask:bulk_transfer_task];
    bulk_transfer_task = UIBackgroundTaskInvalid;
  }
#endif
}

#endif
