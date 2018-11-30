/* screencast-dummy-provider.c
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
#include "screencast-dummy-provider.h"
#include "screencast-dummy-wfd-sink.h"

struct _ScreencastDummyProvider
{
  GObject     parent_instance;

  GPtrArray  *sinks;
};

static void screencast_dummy_provider_provider_iface_init (ScreencastProviderIface *iface);
static GList * screencast_dummy_provider_provider_get_sinks (ScreencastProvider *provider);

G_DEFINE_TYPE_EXTENDED (ScreencastDummyProvider, screencast_dummy_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SCREENCAST_TYPE_PROVIDER,
                                               screencast_dummy_provider_provider_iface_init);
                       )

static void
screencast_dummy_provider_finalize (GObject *object)
{
  ScreencastDummyProvider *dummy_provider = SCREENCAST_DUMMY_PROVIDER (object);

  g_clear_pointer (&dummy_provider->sinks, g_ptr_array_unref);

  G_OBJECT_CLASS (screencast_dummy_provider_parent_class)->finalize (object);
}

static void
screencast_dummy_provider_class_init (ScreencastDummyProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = screencast_dummy_provider_finalize;
}

static void
screencast_dummy_provider_init (ScreencastDummyProvider *dummy_provider)
{
  dummy_provider->sinks = g_ptr_array_new_with_free_func (g_object_unref);

  g_ptr_array_add (dummy_provider->sinks, screencast_dummy_wfd_sink_new ());
}

/******************************************************************
* ScreencastProvider interface implementation
******************************************************************/

static void
screencast_dummy_provider_provider_iface_init (ScreencastProviderIface *iface)
{
  iface->get_sinks = screencast_dummy_provider_provider_get_sinks;
}

static GList *
screencast_dummy_provider_provider_get_sinks (ScreencastProvider *provider)
{
  ScreencastDummyProvider *dummy_provider = SCREENCAST_DUMMY_PROVIDER (provider);
  GList *res = NULL;

  for (gint i = 0; i < dummy_provider->sinks->len; i++)
    res = g_list_prepend (res, dummy_provider->sinks->pdata[i]);

  return res;
}

ScreencastDummyProvider *
screencast_dummy_provider_new (void)
{
  return g_object_new (SCREENCAST_TYPE_DUMMY_PROVIDER,
                       NULL);
}
