#pragma once
// AVAudioSession adapter (iOS only). Owns category/mode configuration, permission,
// and translates system notifications into plain callbacks. Contains no DSP.

#import <Foundation/Foundation.h>

#import "platform/ios/AtmosTypes.h"

NS_ASSUME_NONNULL_BEGIN

@protocol AtmosAudioSessionDelegate <NSObject>
- (void)audioSessionInterruptionBegan;
- (void)audioSessionInterruptionEndedShouldResume:(BOOL)shouldResume;
// oldDeviceUnavailable == YES when headphones were unplugged (Apple guideline: pause).
- (void)audioSessionRouteChanged:(AtmosRouteInfo *)route oldDeviceUnavailable:(BOOL)oldDeviceUnavailable;
- (void)audioSessionMediaServicesWereReset;
@end

@interface AtmosAudioSession : NSObject
@property (nonatomic, weak, nullable) id<AtmosAudioSessionDelegate> delegate;

- (AtmosMicPermission)micPermission;
- (void)requestMicPermission:(void (^)(BOOL granted))completion; // completion on main queue

// Configure for playback only, or play-and-record when the mic is wanted.
- (BOOL)configureForMicrophone:(BOOL)withMic error:(NSError **)error;
- (BOOL)setActive:(BOOL)active error:(NSError **)error;
- (AtmosRouteInfo *)currentRoute;
@end

NS_ASSUME_NONNULL_END
