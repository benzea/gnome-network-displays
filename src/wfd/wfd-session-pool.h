#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <gst/rtsp-server/rtsp-session-pool.h>
#pragma GCC diagnostic pop

G_BEGIN_DECLS

#define WFD_TYPE_SESSION_POOL (wfd_session_pool_get_type())

G_DECLARE_FINAL_TYPE (WfdSessionPool, wfd_session_pool, WFD, SESSION_POOL, GstRTSPSessionPool)

WfdSessionPool *wfd_session_pool_new (void);

G_END_DECLS
