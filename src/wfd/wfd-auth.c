#include "wfd-auth.h"

static gboolean warned_missing_session_header = FALSE;

struct _WfdAuth
{
  GstRTSPAuth parent_instance;
};

G_DEFINE_TYPE (WfdAuth, wfd_auth, GST_TYPE_RTSP_AUTH)

WfdAuth *
wfd_auth_new (void)
{
  return g_object_new (WFD_TYPE_AUTH, NULL);
}

static GstRTSPFilterResult
wfd_auth_find_session_filter_func (GstRTSPClient  *client,
                                   GstRTSPSession *sess,
                                   gpointer        user_data)
{
  *((GstRTSPSession**) user_data) = sess;

  return GST_RTSP_FILTER_KEEP;
}

static gboolean
wfd_auth_check (GstRTSPAuth * auth, GstRTSPContext * ctx, const gchar * check)
{
  /* This is a NASTY hack. Because this is the only place where we can hook
   * into the PLAY command handling, we need to do that here.
   * We have some sinks that just never bother to send the Session header even
   * if they are sending e.g. the PLAY command. Now, obviously the
   * specification mandates that, but what can we do ...
   *
   * The following tries to work around it. i.e. in certain situations when
   * we know a Session header should be there and we also see that it isn't
   * we just update the GstRTSPContext to the correct state so that it looks
   * like the correct header had been there.
   * I feel dirty for doing this ...
   *
   * See https://github.com/benzea/gnome-network-displays/issues/78
   * and https://gitlab.freedesktop.org/gstreamer/gst-rtsp-server/issues/99
   */
  if (!ctx->session)
    {
      if (ctx->method == GST_RTSP_PLAY ||
          ctx->method == GST_RTSP_PAUSE ||
          ctx->method == GST_RTSP_TEARDOWN)
        {
          if (!warned_missing_session_header)
            {
              warned_missing_session_header = TRUE;
              g_warning("The WFD Client did not send a session header for a command even though it should have! Working around this.");
            }

          /* OK, we are ready to do our magic. */
          gst_rtsp_client_session_filter (ctx->client, wfd_auth_find_session_filter_func, &(ctx->session));
        }
    }

  /* Fall back to default implementation. */
  return GST_RTSP_AUTH_CLASS(wfd_auth_parent_class)->check (auth, ctx, check);
}

static void
wfd_auth_class_init (WfdAuthClass *klass)
{
  GstRTSPAuthClass *rtsp_auth = GST_RTSP_AUTH_CLASS (klass);

  rtsp_auth->check = wfd_auth_check;
}

static void
wfd_auth_init (WfdAuth *self)
{
  gst_rtsp_auth_set_supported_methods (GST_RTSP_AUTH (self), GST_RTSP_AUTH_NONE);
}
