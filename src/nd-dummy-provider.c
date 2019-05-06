/* nd-dummy-provider.c
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
#include "nd-dummy-provider.h"
#include "nd-dummy-wfd-sink.h"

struct _NdDummyProvider
{
  GObject    parent_instance;

  gboolean   discover;

  GPtrArray *sinks;
};

enum {
  PROP_DISCOVER = 1,

  PROP_LAST = PROP_DISCOVER,
};

static void nd_dummy_provider_provider_iface_init (NdProviderIface *iface);
static GList * nd_dummy_provider_provider_get_sinks (NdProvider *provider);

G_DEFINE_TYPE_EXTENDED (NdDummyProvider, nd_dummy_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (ND_TYPE_PROVIDER,
                                               nd_dummy_provider_provider_iface_init);
                       )

static void
nd_dummy_provider_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  NdDummyProvider *self = ND_DUMMY_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_DISCOVER:
      g_value_set_boolean (value, self->discover);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
nd_dummy_provider_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  NdDummyProvider *self = ND_DUMMY_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_DISCOVER:
      self->discover = g_value_get_boolean (value);

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
nd_dummy_provider_finalize (GObject *object)
{
  NdDummyProvider *dummy_provider = ND_DUMMY_PROVIDER (object);

  g_clear_pointer (&dummy_provider->sinks, g_ptr_array_unref);

  G_OBJECT_CLASS (nd_dummy_provider_parent_class)->finalize (object);
}

static void
nd_dummy_provider_class_init (NdDummyProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = nd_dummy_provider_get_property;
  object_class->set_property = nd_dummy_provider_set_property;
  object_class->finalize = nd_dummy_provider_finalize;

  g_object_class_override_property (object_class, PROP_DISCOVER, "discover");
}

static void
nd_dummy_provider_init (NdDummyProvider *dummy_provider)
{
  dummy_provider->discover = TRUE;
  dummy_provider->sinks = g_ptr_array_new_with_free_func (g_object_unref);

  g_ptr_array_add (dummy_provider->sinks, nd_dummy_wfd_sink_new ());
}

/******************************************************************
* NdProvider interface implementation
******************************************************************/

static void
nd_dummy_provider_provider_iface_init (NdProviderIface *iface)
{
  iface->get_sinks = nd_dummy_provider_provider_get_sinks;
}

static GList *
nd_dummy_provider_provider_get_sinks (NdProvider *provider)
{
  NdDummyProvider *dummy_provider = ND_DUMMY_PROVIDER (provider);
  GList *res = NULL;

  for (gint i = 0; i < dummy_provider->sinks->len; i++)
    res = g_list_prepend (res, dummy_provider->sinks->pdata[i]);

  return res;
}

NdDummyProvider *
nd_dummy_provider_new (void)
{
  return g_object_new (ND_TYPE_DUMMY_PROVIDER,
                       NULL);
}
