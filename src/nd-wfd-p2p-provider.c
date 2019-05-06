/* nd-wfd-p2p-provider.c
 *
 * Copyright 2018 Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gnome-network-displays-config.h"
#include "nd-wfd-p2p-provider.h"
#include "nd-wfd-p2p-sink.h"

struct _NdWFDP2PProvider
{
  GObject    parent_instance;

  GPtrArray *sinks;

  NMClient  *nm_client;
  NMDevice  *nm_device;

  gboolean   discover;
  guint      p2p_find_source_id;
};

enum {
  PROP_CLIENT = 1,
  PROP_DEVICE,

  PROP_DISCOVER,

  PROP_LAST = PROP_DISCOVER,
};

static void nd_wfd_p2p_provider_provider_iface_init (NdProviderIface *iface);
static GList * nd_wfd_p2p_provider_provider_get_sinks (NdProvider *provider);

G_DEFINE_TYPE_EXTENDED (NdWFDP2PProvider, nd_wfd_p2p_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (ND_TYPE_PROVIDER,
                                               nd_wfd_p2p_provider_provider_iface_init);
                       )

static GParamSpec * props[PROP_LAST] = { NULL, };

static void
peer_added_cb (NdWFDP2PProvider *provider, NMWifiP2PPeer *peer, NMDevice *device)
{
  NdWFDP2PSink *sink = NULL;
  GBytes *wfd_ies;

  wfd_ies = nm_wifi_p2p_peer_get_wfd_ies (peer);

  /* Assume this is not a WFD Peer if there are no WFDIEs set. */
  if (!wfd_ies || g_bytes_get_size (wfd_ies) == 0)
    return;

  g_debug ("WFDP2PProvider: Found a new sink with peer %p on device %p", peer, device);

  sink = nd_wfd_p2p_sink_new (provider->nm_client, provider->nm_device, peer);

  g_ptr_array_add (provider->sinks, sink);
  g_signal_emit_by_name (provider, "sink-added", sink);
}

static void
peer_removed_cb (NdWFDP2PProvider *provider, NMWifiP2PPeer *peer, NMDevice *device)
{
  g_debug ("WFDP2PProvider: Peer removed");

  for (gint i = 0; i < provider->sinks->len; i++)
    {
      g_autoptr(NdWFDP2PSink) sink = g_object_ref (g_ptr_array_index (provider->sinks, i));

      if (nd_wfd_p2p_provider_get_device (provider) != device)
        continue;

      g_ptr_array_remove_index (provider->sinks, i);
      g_signal_emit_by_name (provider, "sink-removed", sink);
      break;
    }
}

static void
nd_wfd_p2p_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  NdWFDP2PProvider *provider = ND_WFD_P2P_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_assert (provider->nm_client == NULL);
      g_value_set_object (value, provider->nm_client);
      break;

    case PROP_DEVICE:
      g_assert (provider->nm_device == NULL);
      g_value_set_object (value, provider->nm_device);
      break;

    case PROP_DISCOVER:
      g_value_set_boolean (value, provider->discover);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
device_restart_find_timeout (gpointer user_data)
{
  NdWFDP2PProvider *provider = ND_WFD_P2P_PROVIDER (user_data);

  nm_device_wifi_p2p_start_find (NM_DEVICE_WIFI_P2P (provider->nm_device), NULL, NULL, NULL, NULL);

  return G_SOURCE_CONTINUE;
}

static void
nd_wfd_p2p_provider_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  NdWFDP2PProvider *provider = ND_WFD_P2P_PROVIDER (object);
  const GPtrArray *peers;

  switch (prop_id)
    {
    case PROP_CLIENT:
      /* Construct only */
      provider->nm_client = g_value_dup_object (value);
      break;

    case PROP_DEVICE:
      /* Construct only */
      provider->nm_device = g_value_dup_object (value);

      g_signal_connect_object (provider->nm_device,
                               "peer-added",
                               (GCallback) peer_added_cb,
                               provider,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (provider->nm_device,
                               "peer-removed",
                               (GCallback) peer_removed_cb,
                               provider,
                               G_CONNECT_SWAPPED);

      if (provider->discover)
        {
          nm_device_wifi_p2p_start_find (NM_DEVICE_WIFI_P2P (provider->nm_device), NULL, NULL, NULL, NULL);
          provider->p2p_find_source_id = g_timeout_add_seconds (20, device_restart_find_timeout, provider);
        }

      peers = nm_device_wifi_p2p_get_peers (NM_DEVICE_WIFI_P2P (provider->nm_device));
      for (gint i = 0; i < peers->len; i++)
        peer_added_cb (provider, g_ptr_array_index (peers, i), provider->nm_device);

      break;

    case PROP_DISCOVER:
      provider->discover = g_value_get_boolean (value);
      g_debug ("WfdP2PProvider: Discover is now set to %d", provider->discover);

      if (provider->discover)
        {
          nm_device_wifi_p2p_start_find (NM_DEVICE_WIFI_P2P (provider->nm_device), NULL, NULL, NULL, NULL);
          if (!provider->p2p_find_source_id)
            provider->p2p_find_source_id = g_timeout_add_seconds (20, device_restart_find_timeout, provider);
        }
      else
        {
          if (provider->p2p_find_source_id)
            g_source_remove (provider->p2p_find_source_id);
          provider->p2p_find_source_id = 0;

          nm_device_wifi_p2p_stop_find (NM_DEVICE_WIFI_P2P (provider->nm_device), NULL, NULL, NULL);
        }

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
nd_wfd_p2p_provider_finalize (GObject *object)
{
  NdWFDP2PProvider *provider = ND_WFD_P2P_PROVIDER (object);

  if (provider->p2p_find_source_id)
    g_source_remove (provider->p2p_find_source_id);
  provider->p2p_find_source_id = 0;

  g_ptr_array_free (provider->sinks, TRUE);
  provider->sinks = NULL;
  g_clear_object (&provider->nm_client);
  g_clear_object (&provider->nm_device);

  G_OBJECT_CLASS (nd_wfd_p2p_provider_parent_class)->finalize (object);
}

static void
nd_wfd_p2p_provider_class_init (NdWFDP2PProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = nd_wfd_p2p_provider_get_property;
  object_class->set_property = nd_wfd_p2p_provider_set_property;
  object_class->finalize = nd_wfd_p2p_provider_finalize;

  props[PROP_CLIENT] =
    g_param_spec_object ("client", "Client",
                         "The NMClient used to find the sink.",
                         NM_TYPE_CLIENT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_DEVICE] =
    g_param_spec_object ("device", "Device",
                         "The NMDevice the sink was found on.",
                         NM_TYPE_DEVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);

  g_object_class_override_property (object_class, PROP_DISCOVER, "discover");
}

static void
nd_wfd_p2p_provider_init (NdWFDP2PProvider *provider)
{
  provider->sinks = g_ptr_array_new_with_free_func (g_object_unref);
}

/******************************************************************
* NdProvider interface implementation
******************************************************************/

static void
nd_wfd_p2p_provider_provider_iface_init (NdProviderIface *iface)
{
  iface->get_sinks = nd_wfd_p2p_provider_provider_get_sinks;
}

static GList *
nd_wfd_p2p_provider_provider_get_sinks (NdProvider *provider)
{
  NdWFDP2PProvider *wfd_p2p_provider = ND_WFD_P2P_PROVIDER (provider);
  GList *res = NULL;

  for (gint i = 0; i < wfd_p2p_provider->sinks->len; i++)
    res = g_list_prepend (res, g_ptr_array_index (wfd_p2p_provider->sinks, i));

  return res;
}

/******************************************************************
* NdWFDP2PProvider public functions
******************************************************************/

/**
 * nd_wfd_p2p_provider_get_client
 * @provider: a #NdWFDP2PProvider
 *
 * Retrieve the #NMClient used to find the device.
 *
 * Returns: (transfer none): The #NMClient
 */
NMClient *
nd_wfd_p2p_provider_get_client (NdWFDP2PProvider *provider)
{
  return provider->nm_client;
}

/**
 * nd_wfd_p2p_provider_get_device
 * @provider: a #NdWFDP2PProvider
 *
 * Retrieve the #NMDevice the provider is for.
 *
 * Returns: (transfer none): The #NMDevice
 */
NMDevice *
nd_wfd_p2p_provider_get_device (NdWFDP2PProvider *provider)
{
  return provider->nm_device;
}


NdWFDP2PProvider *
nd_wfd_p2p_provider_new (NMClient *client, NMDevice *device)
{
  return g_object_new (ND_TYPE_WFD_P2P_PROVIDER,
                       "client", client,
                       "device", device,
                       NULL);
}
