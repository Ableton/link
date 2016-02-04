// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#if defined __cplusplus
extern "C" {
#endif

typedef enum  {
    ABLLINK_NOTIFY_ENABLED,
    ABLLINK_NOTIFY_DISABLED,
    ABLLINK_NOTIFY_DISCOVERY,
    ABLLINK_NOTIFY_CONNECTED,
} ABLLinkNotificationType;

void ABLLinkNotificationsShowMessage(ABLLinkNotificationType message, size_t peers = 0);

#if defined __cplusplus
};
#endif
