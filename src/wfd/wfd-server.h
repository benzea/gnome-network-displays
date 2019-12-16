#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <gst/rtsp-server/rtsp-server.h>
#pragma GCC diagnostic pop

G_BEGIN_DECLS

#define WFD_TYPE_SERVER (wfd_server_get_type ())

G_DECLARE_FINAL_TYPE (WfdServer, wfd_server, WFD, SERVER, GstRTSPServer)

WfdServer * wfd_server_new (void);
void wfd_server_purge (WfdServer *self);

G_END_DECLS
