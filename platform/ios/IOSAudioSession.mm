#import "platform/ios/IOSAudioSession.h"

#import <AVFoundation/AVFoundation.h>

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

@implementation AtmosAudioSession

- (instancetype)init {
    if ((self = [super init])) {
        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        AVAudioSession *s = [AVAudioSession sharedInstance];
        [nc addObserver:self selector:@selector(onInterruption:) name:AVAudioSessionInterruptionNotification object:s];
        [nc addObserver:self selector:@selector(onRouteChange:) name:AVAudioSessionRouteChangeNotification object:s];
        [nc addObserver:self selector:@selector(onReset:) name:AVAudioSessionMediaServicesWereResetNotification object:s];
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (AtmosMicPermission)micPermission {
    switch ([AVAudioApplication sharedInstance].recordPermission) {
        case AVAudioApplicationRecordPermissionGranted: return AtmosMicPermissionGranted;
        case AVAudioApplicationRecordPermissionDenied: return AtmosMicPermissionDenied;
        default: return AtmosMicPermissionUndetermined;
    }
}

- (void)requestMicPermission:(void (^)(BOOL))completion {
    [AVAudioApplication requestRecordPermissionWithCompletionHandler:^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{ completion(granted); });
    }];
}

- (BOOL)configureForMicrophone:(BOOL)withMic error:(NSError **)error {
    AVAudioSession *s = [AVAudioSession sharedInstance];
    BOOL ok;
    if (withMic) {
        // Measurement mode disables voice processing/AGC so environmental sound is
        // captured as-is. A2DP keeps high-quality Bluetooth output (input then stays
        // on the built-in mic). DefaultToSpeaker avoids the quiet receiver.
        ok = [s setCategory:AVAudioSessionCategoryPlayAndRecord
                       mode:AVAudioSessionModeMeasurement
                    options:AVAudioSessionCategoryOptionAllowBluetoothA2DP |
                            AVAudioSessionCategoryOptionDefaultToSpeaker
                      error:error];
    } else {
        ok = [s setCategory:AVAudioSessionCategoryPlayback mode:AVAudioSessionModeDefault options:0 error:error];
    }
    if (!ok) return NO;
    [s setPreferredSampleRate:48000.0 error:nil];       // preference only; hardware may differ
    [s setPreferredIOBufferDuration:0.005 error:nil];   // ~256 frames @48k; OS may choose larger
    return YES;
}

- (BOOL)setActive:(BOOL)active error:(NSError **)error {
    return [[AVAudioSession sharedInstance] setActive:active
                                          withOptions:active ? 0 : AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                                error:error];
}

static AtmosRouteKind kindForPort(AVAudioSessionPort port) {
    if ([port isEqualToString:AVAudioSessionPortBuiltInSpeaker] || [port isEqualToString:AVAudioSessionPortBuiltInReceiver])
        return AtmosRouteKindSpeaker;
    if ([port isEqualToString:AVAudioSessionPortHeadphones] || [port isEqualToString:AVAudioSessionPortUSBAudio] ||
        [port isEqualToString:AVAudioSessionPortLineOut])
        return AtmosRouteKindWired;
    if ([port isEqualToString:AVAudioSessionPortBluetoothA2DP] || [port isEqualToString:AVAudioSessionPortBluetoothHFP] ||
        [port isEqualToString:AVAudioSessionPortBluetoothLE])
        return AtmosRouteKindBluetooth;
    if ([port isEqualToString:AVAudioSessionPortAirPlay]) return AtmosRouteKindAirPlay;
    return AtmosRouteKindOther;
}

- (AtmosRouteInfo *)currentRoute {
    AVAudioSession *s = [AVAudioSession sharedInstance];
    AtmosRouteInfo *info = [AtmosRouteInfo new];
    AVAudioSessionPortDescription *out = s.currentRoute.outputs.firstObject;
    AVAudioSessionPortDescription *in = s.currentRoute.inputs.firstObject;
    info.outputName = out.portName ?: @"None";
    info.inputName = in.portName ?: @"None";
    info.kind = out ? kindForPort(out.portType) : AtmosRouteKindOther;
    info.sampleRate = s.sampleRate;
    info.ioBufferDuration = s.IOBufferDuration;
    info.reportedInputLatency = s.inputLatency;
    info.reportedOutputLatency = s.outputLatency;
    return info;
}

- (void)onInterruption:(NSNotification *)n {
    const NSUInteger type = [n.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (type == AVAudioSessionInterruptionTypeBegan) {
            [self.delegate audioSessionInterruptionBegan];
        } else {
            const NSUInteger opts = [n.userInfo[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
            [self.delegate audioSessionInterruptionEndedShouldResume:(opts & AVAudioSessionInterruptionOptionShouldResume) != 0];
        }
    });
}

- (void)onRouteChange:(NSNotification *)n {
    const NSUInteger reason = [n.userInfo[AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue];
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.delegate audioSessionRouteChanged:[self currentRoute]
                           oldDeviceUnavailable:reason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable];
    });
}

- (void)onReset:(NSNotification *)n {
    dispatch_async(dispatch_get_main_queue(), ^{ [self.delegate audioSessionMediaServicesWereReset]; });
}

@end
