#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <gst/rtsp-server/rtsp-auth.h>
#pragma GCC diagnostic pop

G_BEGIN_DECLS

#define WFD_TYPE_AUTH (wfd_auth_get_type())

G_DECLARE_FINAL_TYPE (WfdAuth, wfd_auth, WFD, AUTH, GstRTSPAuth)

WfdAuth *wfd_auth_new (void);

G_END_DECLS
