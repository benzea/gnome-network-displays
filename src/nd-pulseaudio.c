#include "nd-pulseaudio.h"
#include <pulse/glib-mainloop.h>
#include <pulse/pulseaudio.h>

G_DEFINE_AUTOPTR_CLEANUP_FUNC (pa_proplist, pa_proplist_free)

#define ND_PA_SINK "gnome_screencast"
#define ND_PA_MONITOR ND_PA_SINK ".monitor"

struct _NdPulseaudio
{
  GObject           parent_instance;

  GTask            *init_task;

  pa_glib_mainloop *mainloop;
  pa_mainloop_api  *mainloop_api;
  pa_context       *context;
  guint             null_module_idx;

  pa_operation     *operation;
};

static void      nd_pulseaudio_async_initable_iface_init (GAsyncInitableIface *iface);
static void      nd_pulseaudio_async_initable_init_async (GAsyncInitable     *initable,
                                                          int                 io_priority,
                                                          GCancellable       *cancellable,
                                                          GAsyncReadyCallback callback,
                                                          gpointer            user_data);
static gboolean nd_pulseaudio_async_initable_init_finish (GAsyncInitable *initable,
                                                          GAsyncResult   *res,
                                                          GError        **error);

G_DEFINE_TYPE_EXTENDED (NdPulseaudio, nd_pulseaudio, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               nd_pulseaudio_async_initable_iface_init);
                       )

static void
nd_pulseaudio_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = nd_pulseaudio_async_initable_init_async;
  iface->init_finish = nd_pulseaudio_async_initable_init_finish;
}

static gboolean
return_idle_cb (gpointer user_data)
{
  GTask *task = user_data;
  GError *error = g_object_steal_data (G_OBJECT (task), "result");

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);

  return G_SOURCE_REMOVE;
}

/* Helper function to return an error from idle */
static void
return_idle_error (GTask *task, GError *error)
{
  g_assert (error);
  g_object_set_data_full (G_OBJECT (task), "result", error, (GDestroyNotify) g_error_free);
  g_idle_add (return_idle_cb, g_object_ref (task));
}

static void
return_idle_success (GTask *task)
{
  g_idle_add (return_idle_cb, g_object_ref (task));
}

static void
on_pa_null_module_loaded (pa_context *c,
                          uint32_t    idx,
                          void       *userdata)
{
  NdPulseaudio *self = ND_PULSEAUDIO (userdata);

  if (idx == PA_INVALID_INDEX)
    {
      g_debug ("NdPulseaudio: Module load failed!");
      return_idle_error (self->init_task,
                         g_error_new (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Failed to load NULL module for screencast PA sink"));
      g_clear_object (&self->init_task);
      return;
    }

  g_debug ("NdPulseaudio: Module loaded, we are ready to grab audio! ");
  self->null_module_idx = idx;
  return_idle_success (self->init_task);
  g_clear_object (&self->init_task);
}

static void
on_pa_nd_sink_got_info (pa_context         *c,
                        const pa_sink_info *i,
                        int                 eol,
                        void               *userdata)
{
  NdPulseaudio *self = ND_PULSEAUDIO (userdata);

  /* eol is negative for error, and 1 for end of list (with NULL sink info then) */

  /* We are only ever called once, as we cancel the operation
   * if we get what we want. */
  if (eol < 0 && pa_context_errno (self->context) != PA_ERR_NOENTITY)
    {
      g_debug ("NdPulseaudio: Error querying sink info");
      return_idle_error (self->init_task,
                         g_error_new (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Error querying sink from PA: %s",
                                      pa_strerror (pa_context_errno (self->context))));
      g_clear_object (&self->init_task);
      return;
    }

  if (eol == 0)
    {
      /* This is the case that we could query the sink, so it seems
       * like it exists already. Just double check things, and
       * return successful initialization.
       */

      /* Cancel the operation as we would be called a second time for
       * the list end which would cause am immediate successfull return. */
      pa_operation_cancel (self->operation);
      g_clear_pointer (&self->operation, pa_operation_unref);

      g_debug ("NdPulseaudio: Error querying sink info");
      g_debug ("NdPulseaudio: Got a sink info for the expected name");

      return_idle_success (self->init_task);
      g_clear_object (&self->init_task);
      return;
    }

  g_debug ("NdPulseaudio: Sink does not exist yet, loading module");

  /* We have reached the list end without being cancelled first.
   * This means no screencast sink exist, and we need to create it. */
  self->operation = pa_context_load_module (self->context,
                                            "module-null-sink",
                                            "sink_name=gnome_network_displays "
                                            "rate=48000 "
                                            "sink_properties=device.description=\"Network-Displays\""
                                            "device.class=\"sound\""
                                            "device.icon_name=\"network-wireless\"",
                                            on_pa_null_module_loaded,
                                            self);
}

static void
nd_pulseaudio_state_cb (pa_context *context,
                        void       *user_data)
{
  NdPulseaudio *self = ND_PULSEAUDIO (user_data);

  switch (pa_context_get_state (context))
    {
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;

    case PA_CONTEXT_READY:
      if (!self->init_task)
        return;

      g_debug ("NdPulseaudio: Querying sink info by name");
      self->operation = pa_context_get_sink_info_by_name (self->context,
                                                          ND_PA_SINK,
                                                          on_pa_nd_sink_got_info,
                                                          self);
      break;

    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      if (!self->init_task)
        return;

      g_debug ("NdPulseaudio: PA context went into failed state during init");
      return_idle_error (self->init_task,
                         g_error_new (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "PA failed"));
      g_clear_object (&self->init_task);
      break;

    default:
      /* FIXME: */
      break;
    }
}

static void
nd_pulseaudio_async_initable_init_async (GAsyncInitable     *initable,
                                         int                 io_priority,
                                         GCancellable       *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer            user_data)
{
  NdPulseaudio *self = ND_PULSEAUDIO (initable);

  g_autoptr(pa_proplist) proplist = NULL;
  gint res;

  self->mainloop = pa_glib_mainloop_new (g_main_context_default ());
  self->mainloop_api = pa_glib_mainloop_get_api (self->mainloop);

  proplist = pa_proplist_new ();
  pa_proplist_sets (proplist, PA_PROP_APPLICATION_NAME, "GNOME Network Displays");
  pa_proplist_sets (proplist, PA_PROP_APPLICATION_ID, "org.gnome.NetworkDisplays");
  /* pa_proplist_sets (proplist, PA_PROP_APPLICATION_ICON_NAME, ); */

  self->context = pa_context_new_with_proplist (self->mainloop_api, NULL, proplist);

  /* Create our task; we currently don't handle cancellation internally */
  self->init_task = g_task_new (initable, cancellable, callback, user_data);

  pa_context_set_state_callback (self->context,
                                 nd_pulseaudio_state_cb,
                                 self);

  res = pa_context_connect (self->context, NULL, (pa_context_flags_t) PA_CONTEXT_NOFLAGS, NULL);
  if (res < 0)
    {
      g_debug ("NdPulseaudio: Error querying sink info");
      g_task_return_new_error (self->init_task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Error connecting to PA: %s",
                               pa_strerror (pa_context_errno (self->context)));
      g_clear_object (&self->init_task);
      return;
    }

  /* Wait for us to be connected. */
}

static gboolean
nd_pulseaudio_async_initable_init_finish (GAsyncInitable *initable,
                                          GAsyncResult   *res,
                                          GError        **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}



NdPulseaudio *
nd_pulseaudio_new (void)
{
  return g_object_new (ND_TYPE_PULSEAUDIO, NULL);
}

static void
nd_pulseaudio_finalize (GObject *object)
{
  NdPulseaudio *self = (NdPulseaudio *) object;

  if (self->init_task)
    {
      g_task_return_new_error (self->init_task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "Object finalised, async init was cancelled.");
      g_clear_object (&self->init_task);
    }

  if (self->operation)
    pa_operation_cancel (self->operation);
  g_clear_pointer (&self->operation, pa_operation_unref);

  g_clear_pointer (&self->context, pa_context_unref);
  self->mainloop_api = NULL;
  g_clear_pointer (&self->mainloop, pa_glib_mainloop_free);

  G_OBJECT_CLASS (nd_pulseaudio_parent_class)->finalize (object);
}

static void
nd_pulseaudio_class_init (NdPulseaudioClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nd_pulseaudio_finalize;
}

static void
nd_pulseaudio_init (NdPulseaudio *self)
{
  self->null_module_idx = PA_INVALID_INDEX;
}

GstElement *
nd_pulseaudio_get_source (NdPulseaudio *self)
{
  g_autoptr(GstElement) src = NULL;

  g_assert (self->init_task == NULL);
  g_assert (self->context != NULL);

  src = gst_element_factory_make ("pulsesrc", "pulseaudio-source");

  g_object_set (src,
                "device", ND_PA_MONITOR,
                "client-name", "GNOME Network Displays Audio Grabber",
                "do-timestamp", TRUE,
                "server", pa_context_get_server (self->context),
                NULL);

  return g_steal_pointer (&src);
}
