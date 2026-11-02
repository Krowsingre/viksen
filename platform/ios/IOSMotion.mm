#import "platform/ios/IOSMotion.h"

#import <CoreMotion/CoreMotion.h>

@implementation AtmosMotionSource {
    CMMotionManager *_manager;
    NSTimeInterval _lastTimestamp;
}

- (instancetype)init {
    if ((self = [super init])) _manager = [CMMotionManager new];
    return self;
}

- (BOOL)available { return _manager.accelerometerAvailable; }
- (BOOL)running { return _manager.accelerometerActive; }

- (void)startWithRateHz:(double)hz handler:(AtmosMotionHandler)handler {
    if (!_manager.accelerometerAvailable) return;
    [self stop];
    _manager.accelerometerUpdateInterval = 1.0 / hz;
    _lastTimestamp = 0;
    __weak AtmosMotionSource *weakSelf = self;
    [_manager startAccelerometerUpdatesToQueue:[NSOperationQueue mainQueue]
                                   withHandler:^(CMAccelerometerData *data, NSError *error) {
        AtmosMotionSource *s = weakSelf;
        if (!s || !data || error) return;
        const NSTimeInterval dt = s->_lastTimestamp > 0 ? data.timestamp - s->_lastTimestamp : 1.0 / hz;
        s->_lastTimestamp = data.timestamp;
        handler(data.acceleration.x, data.acceleration.y, data.acceleration.z, dt > 0 && dt < 1.0 ? dt : 1.0 / hz);
    }];
}

- (void)stop {
    if (_manager.accelerometerActive) [_manager stopAccelerometerUpdates];
}

@end
