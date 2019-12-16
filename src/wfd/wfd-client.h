#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <gst/rtsp-server/rtsp-client.h>
#pragma GCC diagnostic pop

G_BEGIN_DECLS

#define WFD_TYPE_CLIENT (wfd_client_get_type ())
#define WFD_CLIENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WFD_TYPE_CLIENT, WfdClientClass))

G_DECLARE_FINAL_TYPE (WfdClient, wfd_client, WFD, CLIENT, GstRTSPClient)

WfdClient * wfd_client_new (void);
void wfd_client_query_support (WfdClient *self);
void wfd_client_trigger_method (WfdClient   *self,
                                const gchar *method);


G_END_DECLS
