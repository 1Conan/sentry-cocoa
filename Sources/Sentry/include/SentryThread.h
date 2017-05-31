//
//  SentryThread.h
//  Sentry
//
//  Created by Daniel Griesser on 05/05/2017.
//  Copyright © 2017 Sentry. All rights reserved.
//

#import <Foundation/Foundation.h>

#if __has_include(<Sentry/Sentry.h>)
#import <Sentry/SentryDefines.h>
#import <Sentry/SentrySerializable.h>
#else
#import "SentryDefines.h"
#import "SentrySerializable.h"
#endif

NS_ASSUME_NONNULL_BEGIN

@class SentryStacktrace;

NS_SWIFT_NAME(Thread)
@interface SentryThread : NSObject <SentrySerializable>
SENTRY_NO_INIT

@property(nonatomic, copy) NSNumber *threadId;
@property(nonatomic, copy) NSString *_Nullable name;
@property(nonatomic, strong) SentryStacktrace *_Nullable stacktrace;
@property(nonatomic, copy) NSNumber *_Nullable crashed;
@property(nonatomic, copy) NSNumber *_Nullable current;

- (instancetype)initWithThreadId:(NSNumber *)threadId;

@end

NS_ASSUME_NONNULL_END
