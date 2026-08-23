// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LocalNotifications.hpp"

#ifdef HAVE_LOCAL_NOTIFICATIONS

#include "LogFile.hpp"
#include "thread/Mutex.hxx"
#include "util/StaticString.hxx"

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#include <atomic>
#include <chrono>

/**
 * Notifications with this text are suppressed if the same text was
 * posted less than this long ago.  Some message sources repeat
 * themselves with every received NMEA sentence, and unlike the popup
 * message, a notification stays on the lock screen until it is
 * dismissed.
 */
static constexpr auto repeat_timeout = std::chrono::minutes{1};

static std::atomic_bool notifications_enabled{false};
static std::atomic_bool notifications_authorised{false};

/**
 * Is XCSoar currently in the foreground?  This is tracked with
 * observers instead of asking UIApplication, because Post() runs on
 * arbitrary threads while UIApplication may only be used on the main
 * thread.
 */
static std::atomic_bool application_foreground{true};

static Mutex repeat_mutex;
static StaticString<256> last_text;
static std::chrono::steady_clock::time_point last_time;

@interface XCSoarNotificationObserver : NSObject
@end

@implementation XCSoarNotificationObserver

- (instancetype)init
{
  self = [super init];
  if (self) {
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

    [center addObserver:self
               selector:@selector(applicationDidEnterBackground:)
                   name:UIApplicationDidEnterBackgroundNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(applicationWillEnterForeground:)
                   name:UIApplicationWillEnterForegroundNotification
                 object:nil];
  }

  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)applicationDidEnterBackground:(NSNotification *)notification
{
  application_foreground.store(false, std::memory_order_relaxed);
}

- (void)applicationWillEnterForeground:(NSNotification *)notification
{
  application_foreground.store(true, std::memory_order_relaxed);
}

@end

static XCSoarNotificationObserver *observer;

static void
RequestAuthorisation() noexcept
{
  const UNAuthorizationOptions options =
    UNAuthorizationOptionAlert | UNAuthorizationOptionSound;

  [[UNUserNotificationCenter currentNotificationCenter]
    requestAuthorizationWithOptions:options
                  completionHandler:^(BOOL granted, NSError *error) {
      notifications_authorised.store(granted, std::memory_order_relaxed);

      if (error != nil)
        LogFmt("Notifications: authorisation failed: {}",
               [[error localizedDescription] UTF8String]);
      else if (!granted)
        LogString("Notifications: authorisation denied by the user");
    }];
}

void
LocalNotifications::Initialise(bool enabled) noexcept
{
  observer = [[XCSoarNotificationObserver alloc] init];

  application_foreground.store([UIApplication sharedApplication].applicationState
                               != UIApplicationStateBackground,
                               std::memory_order_relaxed);

  SetEnabled(enabled);
}

void
LocalNotifications::Deinitialise() noexcept
{
  notifications_enabled.store(false, std::memory_order_relaxed);

  /* ARC releases the observer, and -dealloc unregisters it */
  observer = nil;
}

void
LocalNotifications::SetEnabled(bool enabled) noexcept
{
  const bool was_enabled =
    notifications_enabled.exchange(enabled, std::memory_order_relaxed);

  /* asking again after the user has decided once does not show another
     dialog, but it does refresh our copy of that decision */
  if (enabled && !was_enabled)
    RequestAuthorisation();
}

/**
 * Has this message just been posted?  Updates the record of the last
 * notification as a side effect.  The body is part of the comparison,
 * because several message sources use a constant title and put the
 * interesting part into the body.
 */
static bool
CheckRepeat(const char *title, const char *body) noexcept
{
  StaticString<256> text;
  text.assign(title);
  if (body != nullptr) {
    text.push_back(' ');
    text.append(body);
  }

  const auto now = std::chrono::steady_clock::now();

  const std::lock_guard lock{repeat_mutex};

  if (last_text == text && now < last_time + repeat_timeout)
    return true;

  last_text = text;
  last_time = now;
  return false;
}

void
LocalNotifications::Post(const char *title, const char *body) noexcept
{
  if (!notifications_enabled.load(std::memory_order_relaxed) ||
      !notifications_authorised.load(std::memory_order_relaxed))
    return;

  if (application_foreground.load(std::memory_order_relaxed))
    return;

  if (title == nullptr || *title == '\0')
    return;

  NSString *title_string = [NSString stringWithUTF8String:title];
  if (title_string == nil)
    return;

  if (CheckRepeat(title, body))
    return;

  UNMutableNotificationContent *content =
    [[UNMutableNotificationContent alloc] init];
  content.title = title_string;

  /* one thread identifier for all XCSoar messages makes the operating
     system group them into a single stack */
  content.threadIdentifier = @"de.xcsoar.message";

  /* the alert sound is left to the operating system; it obeys the
     user's per-app notification settings, which XCSoar's own message
     sounds do not */
  content.sound = [UNNotificationSound defaultSound];

  if (body != nullptr && *body != '\0') {
    NSString *body_string = [NSString stringWithUTF8String:body];
    if (body_string != nil)
      content.body = body_string;
  }

  UNNotificationRequest *request =
    [UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString]
                                         content:content
                                         trigger:nil];

  [[UNUserNotificationCenter currentNotificationCenter]
    addNotificationRequest:request
     withCompletionHandler:^(NSError *error) {
      if (error != nil)
        LogFmt("Notifications: failed to post: {}",
               [[error localizedDescription] UTF8String]);
    }];
}

#endif /* HAVE_LOCAL_NOTIFICATIONS */
