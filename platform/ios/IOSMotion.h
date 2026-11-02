#pragma once
// Core Motion accelerometer adapter (iPhone). Delivers samples in g on the main
// queue. Raw accelerometer access needs no permission prompt. No data is stored.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^AtmosMotionHandler)(double ax, double ay, double az, double dtSeconds);

@interface AtmosMotionSource : NSObject
@property (nonatomic, readonly) BOOL available;
@property (nonatomic, readonly) BOOL running;
- (void)startWithRateHz:(double)hz handler:(AtmosMotionHandler)handler;
- (void)stop;
@end

NS_ASSUME_NONNULL_END
