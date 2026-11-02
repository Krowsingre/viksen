#pragma once
// AVAudioSession adapter (iOS only). Owns category/mode configuration, permission,
// and translates system notifications into plain callbacks. Contains no DSP.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AtmosMicPermission) {
    AtmosMicPermissionUndetermined = 0,
    AtmosMicPermissionDenied = 1,
    AtmosMicPermissionGranted = 2,
};

typedef NS_ENUM(NSInteger, AtmosRouteKind) {
    AtmosRouteKindSpeaker = 0,      // built-in speaker/receiver: live monitoring risks feedback
    AtmosRouteKindWired = 1,        // wired / USB headphones: best for live monitoring
    AtmosRouteKindBluetooth = 2,    // Bluetooth: added latency, mic may be lower quality
    AtmosRouteKindAirPlay = 3,
    AtmosRouteKindOther = 4,
};

@interface AtmosRouteInfo : NSObject
@property (nonatomic, copy) NSString *outputName;
@property (nonatomic, copy) NSString *inputName;
@property (nonatomic) AtmosRouteKind kind;
@property (nonatomic) double sampleRate;
@property (nonatomic) double ioBufferDuration;
@property (nonatomic) double reportedInputLatency;   // seconds, as reported by the OS (not measured)
@property (nonatomic) double reportedOutputLatency;
@end

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
