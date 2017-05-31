//
//  SentryUser.h
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

NS_SWIFT_NAME(User)
@interface SentryUser : NSObject <SentrySerializable>
SENTRY_NO_INIT

@property(nonatomic, copy) NSString *userId;
@property(nonatomic, copy) NSString *_Nullable email;
@property(nonatomic, copy) NSString *_Nullable username;
@property(nonatomic, strong) NSDictionary<NSString *, id <NSSecureCoding>> *_Nullable extra;

- (instancetype)initWithUserId:(NSString *)userId;

@end

NS_ASSUME_NONNULL_END
