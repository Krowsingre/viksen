#import "platform/ios/AtmosTypes.h"

@implementation AtmosRouteInfo
- (instancetype)init {
    if ((self = [super init])) {
        _outputName = @"None";
        _inputName = @"None";
        _kind = AtmosRouteKindOther;
    }
    return self;
}
@end
