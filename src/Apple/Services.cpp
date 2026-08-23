// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef __APPLE__

#include <TargetConditionals.h>
#include "LogFile.hpp"
#include "Services.hpp"
#include "BluetoothHelper.hpp"
#include "thread/Mutex.hxx"
#import <AVFoundation/AVFoundation.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

#include <cassert>

BluetoothHelper *bluetooth_helper;

#if TARGET_OS_IPHONE

/**
 * Serialises audio_vario_session_active against the deactivation of the
 * shared AVAudioSession. Without it, DeactivateAudioSession() could read
 * the flag as false, then have the audio vario start up (setting the
 * flag and activating the session) before its own setActive:NO takes
 * effect, silencing the freshly started vario.
 */
static Mutex audio_session_mutex;

/**
 * Protected by #audio_session_mutex: written from the SDL audio thread
 * (SDLPCMPlayer) and read from the thread calling SoundUtil::Play() and
 * from the AVAudioPlayer delegate callbacks.
 */
static bool audio_vario_session_active = false;

void
SetAudioVarioSessionActive(bool active)
{
  const std::lock_guard lock{audio_session_mutex};
  audio_vario_session_active = active;
}

void
ActivateAudioSession()
{
  NSError *error = nil;
  AVAudioSession *session = [AVAudioSession sharedInstance];

  // (Re-)apply our preferred category and options. SDL's CoreAudio
  // backend may reset these when it (re-)opens the audio device for the
  // audio vario, which would otherwise cause XCSoar to duck other apps'
  // audio.
  [session setCategory:AVAudioSessionCategoryPlayback
           withOptions:AVAudioSessionCategoryOptionMixWithOthers
                 error:&error];
  if (error) {
    LogFmt("AVAudioSession setCategory error: {}",
           [[error localizedDescription] UTF8String]);
    error = nil;
  }

  [session setActive:YES error:&error];
  if (error) {
    LogFmt("AVAudioSession activate error: {}",
           [[error localizedDescription] UTF8String]);
  }
}

void
DeactivateAudioSession()
{
  // hold the lock across the check and the deactivation, so that the
  // audio vario cannot start up in between and get silenced by our
  // setActive:NO
  const std::lock_guard lock{audio_session_mutex};

  if (audio_vario_session_active) {
    // keep the session active while the audio vario's audio device is
    // open: deactivating it would also silence the audio vario, which
    // SDL would not resume on its own
    return;
  }

  NSError *error = nil;
  [[AVAudioSession sharedInstance] setActive:NO
                                   withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                   error:&error];
  if (error) {
    LogFmt("AVAudioSession deactivate error: {}",
           [[error localizedDescription] UTF8String]);
  }
}

#endif

// Initialize apple services - this will be called from the main XCSoar startup
void
InitializeAppleServices()
{
#if TARGET_OS_IPHONE
  ActivateAudioSession();

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
  [[AVAudioSession sharedInstance] setActive:NO
                                   withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                   error:&error];
  if (error) {
    LogFmt("AVAudioSession deinitialize error: {}",
           [[error localizedDescription] UTF8String]);
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
