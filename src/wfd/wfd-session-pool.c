/* nd-meta-provider.c
 *
 * Copyright 2008 Wim Taymans <wim.taymans at gmail.com>
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

#include "wfd-session-pool.h"

struct _WfdSessionPool
{
  GstRTSPSessionPool parent_instance;
};

G_DEFINE_TYPE (WfdSessionPool, wfd_session_pool, GST_TYPE_RTSP_SESSION_POOL)

/* The following is copied from GstRTSPSessionPool, the only change is that
 * we stop after rather than 16 for the session-id.
 * This is required to support some LG TVs which limit the length to 15 bytes
 * (i.e. 16 including a NUL byte).
 */
static const gchar session_id_charset[] =
    { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
  'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
  'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', '-', '_', '.', '+'  /* '$' Live555 in VLC strips off $ chars */
};

static gchar*
create_session_id (GstRTSPSessionPool * pool)
{
  gchar id[15];
  gint i;

  for (i = 0; i < 15; i++) {
    id[i] =
        session_id_charset[g_random_int_range (0,
            G_N_ELEMENTS (session_id_charset))];
  }

  return g_strndup (id, 15);
}

static void
wfd_session_pool_class_init (WfdSessionPoolClass *klass)
{
  GstRTSPSessionPoolClass *parent_class = GST_RTSP_SESSION_POOL_CLASS (klass);

  parent_class->create_session_id = create_session_id;
}

static void
wfd_session_pool_init (WfdSessionPool *self)
{
}

WfdSessionPool *
wfd_session_pool_new (void)
{
  return g_object_new (WFD_TYPE_SESSION_POOL, NULL);
}
