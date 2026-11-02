#pragma once
// Plain Objective-C value types shared by the iPhone and Mac audio adapters and the
// engine bridge. No platform frameworks beyond Foundation.

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

NS_ASSUME_NONNULL_END
