/* screencast-meta-provider.c
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

#include "gnome-screencast-config.h"
#include "screencast-meta-provider.h"
#include "screencast-meta-sink.h"

struct _ScreencastMetaProvider
{
  GObject     parent_instance;

  GHashTable *deduplicate;
  GPtrArray  *sinks;
  GPtrArray  *providers;
};

static void screencast_meta_provider_provider_iface_init (ScreencastProviderIface *iface);
static GList * screencast_meta_provider_provider_get_sinks (ScreencastProvider *provider);

G_DEFINE_TYPE_EXTENDED (ScreencastMetaProvider, screencast_meta_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SCREENCAST_TYPE_PROVIDER,
                                               screencast_meta_provider_provider_iface_init);
                       )


static void
deduplicate_add_meta_sink (ScreencastMetaProvider *meta_provider, ScreencastMetaSink *meta_sink)
{
  g_autoptr(GPtrArray) meta_matches = NULL;

  g_object_get (meta_sink, "matches", &meta_matches, NULL);
  g_assert (meta_matches != NULL);

  for (gint i = 0; i < meta_matches->len; i++)
    {
      gchar *match;
      match = meta_matches->pdata[i];

      g_hash_table_insert (meta_provider->deduplicate, g_strdup (match), meta_sink);
    }
}

static void
provider_sink_added_cb (ScreencastMetaProvider *meta_provider, ScreencastSink *sink, ScreencastProvider *provider)
{
  g_autoptr(GPtrArray) sink_matches = NULL;
  g_autoptr(GPtrArray) meta_sinks = NULL;
  ScreencastMetaSink *meta_sink;
  gchar *match;

  g_object_get (sink, "matches", &sink_matches, NULL);
  g_assert (sink_matches != NULL);

  meta_sinks = g_ptr_array_new_with_free_func (g_object_unref);

  for (gint i = 0; i < sink_matches->len; i++)
    {
      match = sink_matches->pdata[i];

      meta_sink = g_hash_table_lookup (meta_provider->deduplicate, match);

      if (meta_sink)
        g_ptr_array_add (meta_sinks, meta_sink);
    }

  if (meta_sinks->len > 1)
    g_warning ("MetaProvider: Found two meta sinks that belong to the same sink. This should not happen!\n");

  if (meta_sinks->len > 0)
    {
      meta_sink = g_ptr_array_remove_index (meta_sinks, 0);

      while (meta_sinks->len > 0)
        {
          ScreencastMetaSink *merge_meta;
          ScreencastSink *merge_sink;
          merge_meta = g_ptr_array_remove_index_fast (meta_sinks, 0);
          g_signal_emit_by_name (meta_provider, "sink-removed", merge_meta);

          while ((merge_sink = screencast_meta_sink_get_sink (merge_meta)))
            {
              screencast_meta_sink_remove_sink (merge_meta, merge_sink);
              screencast_meta_sink_add_sink (meta_sink, merge_sink);
            }
        }

      screencast_meta_sink_add_sink (meta_sink, sink);
    }
  else
    {
      meta_sink = screencast_meta_sink_new (sink);
      g_signal_emit_by_name (meta_provider, "sink-added", meta_sink);
    }

  /* Add/Update matches in the deduplication dictionary */
  deduplicate_add_meta_sink (meta_provider, meta_sink);
}

static void
provider_sink_removed_cb (ScreencastMetaProvider *meta_provider, ScreencastSink *sink, ScreencastProvider *provider)
{
  g_autoptr(GPtrArray) sink_matches = NULL;
  ScreencastMetaSink *meta_sink = NULL;

  g_object_get (sink, "matches", &sink_matches, NULL);
  g_assert (sink_matches != NULL);

  for (gint i = 0; i < sink_matches->len; i++)
    {
      gchar *match = sink_matches->pdata[i];

      /* Find the first matching meta sink and remove any match*/
      if (meta_sink == NULL)
        meta_sink = g_hash_table_lookup (meta_provider->deduplicate, match);

      g_hash_table_remove (meta_provider->deduplicate, match);
    }

  g_assert (meta_sink != NULL);
  if (screencast_meta_sink_remove_sink (meta_sink, sink))
    {
      /* meta sink is empty, remove it and we are done */
      g_ptr_array_remove (meta_provider->sinks, meta_sink);
      g_signal_emit_by_name (meta_provider, "sink-removed", meta_sink);

      return;
    }

  /* Add/Update matches in the deduplication dictionary as we will
   * have removed too many. */
  deduplicate_add_meta_sink (meta_provider, meta_sink);
}

static void
screencast_meta_provider_finalize (GObject *object)
{
  ScreencastMetaProvider *meta_provider = SCREENCAST_META_PROVIDER (object);

  g_clear_pointer (&meta_provider->deduplicate, g_hash_table_unref);

  g_ptr_array_free (meta_provider->sinks, TRUE);
  meta_provider->sinks = NULL;
  g_ptr_array_free (meta_provider->providers, TRUE);
  meta_provider->providers = NULL;

  /* Rely on provider signals to be disconnected automatically. */

  G_OBJECT_CLASS (screencast_meta_provider_parent_class)->finalize (object);
}

static void
screencast_meta_provider_class_init (ScreencastMetaProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = screencast_meta_provider_finalize;
}

static void
screencast_meta_provider_init (ScreencastMetaProvider *meta_provider)
{
  meta_provider->sinks = g_ptr_array_new_with_free_func (g_object_unref);
  meta_provider->providers = g_ptr_array_new_with_free_func (g_object_unref);

  meta_provider->deduplicate = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

/******************************************************************
* ScreencastProvider interface implementation
******************************************************************/

static void
screencast_meta_provider_provider_iface_init (ScreencastProviderIface *iface)
{
  iface->get_sinks = screencast_meta_provider_provider_get_sinks;
}

static GList *
screencast_meta_provider_provider_get_sinks (ScreencastProvider *provider)
{
  ScreencastMetaProvider *meta_provider = SCREENCAST_META_PROVIDER (provider);
  GList *res = NULL;

  for (gint i = 0; i < meta_provider->sinks->len; i++)
    res = g_list_prepend (res, meta_provider->sinks->pdata[i]);

  return res;
}

/******************************************************************
* ScreencastMetaProvider public functions
******************************************************************/

/**
 * screencast_meta_provider_get_providers
 * @meta_provider: a #ScreencastMetaProvider
 *
 * Retrieve the providers registered with this meta provider.
 *
 * Returns: (transfer container) (element-type ScreencastProvider):
 *    A list of all known providers
 */
GList *
screencast_meta_provider_get_providers (ScreencastMetaProvider *meta_provider)
{
  GList *res = NULL;

  for (gint i = 0; i < meta_provider->providers->len; i++)
    res = g_list_prepend (res, meta_provider->providers->pdata[i]);

  return res;
}

/**
 * screencast_meta_provider_add_provider
 * @meta_provider: a #ScreencastMetaProvider
 * @provider: a #ScreencastProvider
 *
 * Adds the provider to the list of known providers.
 */
void
screencast_meta_provider_add_provider (ScreencastMetaProvider *meta_provider,
                                       ScreencastProvider     *provider)
{
  g_autoptr(GList) list = NULL;
  GList *item;

  g_assert (provider);
  g_assert (!g_ptr_array_find (meta_provider->providers, provider, NULL));

  g_ptr_array_add (meta_provider->providers, g_object_ref (provider));

  g_signal_connect_object (provider,
                           "sink-added",
                           (GCallback) provider_sink_added_cb,
                           meta_provider,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "sink-removed",
                           (GCallback) provider_sink_removed_cb,
                           meta_provider,
                           G_CONNECT_SWAPPED);

  /* Explicitly add all existing sinks */
  list = screencast_provider_get_sinks (provider);
  item = list;
  while (item)
    {
      provider_sink_added_cb (meta_provider, SCREENCAST_SINK (item->data), provider);
      item = item->next;
    }
}

/**
 * screencast_meta_provider_remove_provider
 * @meta_provider: a #ScreencastMetaProvider
 * @provider: a #ScreencastProvider
 *
 * Remove the provider from the list of providers.
 */
void
screencast_meta_provider_remove_provider (ScreencastMetaProvider *meta_provider,
                                          ScreencastProvider     *provider)
{
  g_autoptr(GList) list = NULL;
  GList *item;
  g_assert (provider);

  g_signal_handlers_disconnect_by_data (provider, meta_provider);

  /* Explicitly remove all existing sinks */
  list = screencast_provider_get_sinks (provider);
  item = list;
  while (item)
    {
      provider_sink_removed_cb (meta_provider, SCREENCAST_SINK (item->data), provider);
      item = item->next;
    }

  g_assert (g_ptr_array_remove (meta_provider->providers, provider));
}

ScreencastMetaProvider *
screencast_meta_provider_new (void)
{
  return g_object_new (SCREENCAST_TYPE_META_PROVIDER,
                       NULL);
}
