/* op-support.c 
   Copyright (C) 2002, 2003 g10 Code GmbH

   This file is part of GPGME.
 
   GPGME is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   GPGME is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with GPGME; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "gpgme.h"
#include "context.h"
#include "ops.h"


gpgme_error_t
_gpgme_op_data_lookup (gpgme_ctx_t ctx, ctx_op_data_type type, void **hook,
		       int size, void (*cleanup) (void *))
{
  struct ctx_op_data *data = ctx->op_data;
  while (data && data->type != type)
    data = data->next;
  if (!data)
    {
      if (size < 0)
	{
	  *hook = NULL;
	  return 0;
	}

      data = calloc (1, sizeof (struct ctx_op_data) + size);
      if (!data)
	return GPGME_Out_Of_Core;
      data->next = ctx->op_data;
      data->type = type;
      data->cleanup = cleanup;
      data->hook = ((void *) data) + sizeof (struct ctx_op_data);
      ctx->op_data = data;
    }
  *hook = data->hook;
  return 0;
}


/* type is: 0: asynchronous operation (use global or user event loop).
            1: synchronous operation (always use private event loop).
            2: asynchronous private operation (use private or user
            event loop).  */
gpgme_error_t
_gpgme_op_reset (gpgme_ctx_t ctx, int type)
{
  gpgme_error_t err = 0;
  struct gpgme_io_cbs io_cbs;

  _gpgme_release_result (ctx);

  /* Create an engine object.  */
  _gpgme_engine_release (ctx->engine);
  ctx->engine = NULL;
  err = _gpgme_engine_new (ctx->protocol, &ctx->engine);
  if (err)
    return err;

  if (type == 1 || (type == 2 && !ctx->io_cbs.add))
    {
      /* Use private event loop.  */
      io_cbs.add = _gpgme_add_io_cb;
      io_cbs.add_priv = ctx;
      io_cbs.remove = _gpgme_remove_io_cb;
      io_cbs.event = _gpgme_wait_private_event_cb;
      io_cbs.event_priv = ctx;
    }
  else if (! ctx->io_cbs.add)
    {
      /* Use global event loop.  */
      io_cbs.add = _gpgme_add_io_cb;
      io_cbs.add_priv = ctx;
      io_cbs.remove = _gpgme_remove_io_cb;
      io_cbs.event = _gpgme_wait_global_event_cb;
      io_cbs.event_priv = ctx;
    }
  else
    {
      /* Use user event loop.  */
      io_cbs.add = _gpgme_wait_user_add_io_cb;
      io_cbs.add_priv = ctx;
      io_cbs.remove = _gpgme_wait_user_remove_io_cb;
      io_cbs.event = _gpgme_wait_user_event_cb;
      io_cbs.event_priv = ctx;
    }
  _gpgme_engine_set_io_cbs (ctx->engine, &io_cbs);
  return err;
}


gpgme_error_t
_gpgme_parse_inv_userid (char *args, gpgme_invalid_user_id_t *userid)
{
  gpgme_invalid_user_id_t inv_userid;
  char *tail;
  long int reason;

  inv_userid = malloc (sizeof (*inv_userid));
  if (!inv_userid)
    return GPGME_Out_Of_Core;
  inv_userid->next = NULL;
  errno = 0;
  reason = strtol (args, &tail, 0);
  if (errno || args == tail || *tail != ' ')
    {
      /* The crypto backend does not behave.  */
      free (inv_userid);
      return GPGME_General_Error;
    }

  switch (reason)
    {
    default:
    case 0:
      inv_userid->reason = GPGME_Unknown_Reason;
      break;

    case 1:
      inv_userid->reason = GPGME_Not_Found;
      break;

    case 2:
      inv_userid->reason = GPGME_Ambiguous_Specification;
      break;

    case 3:
      inv_userid->reason = GPGME_Wrong_Key_Usage;
      break;

    case 4:
      inv_userid->reason = GPGME_Key_Revoked;
      break;

    case 5:
      inv_userid->reason = GPGME_Key_Expired;
      break;

    case 6:
      inv_userid->reason = GPGME_No_CRL_Known;
      break;

    case 7:
      inv_userid->reason = GPGME_CRL_Too_Old;
      break;

    case 8:
      inv_userid->reason = GPGME_Policy_Mismatch;
      break;

    case 9:
      inv_userid->reason = GPGME_No_Secret_Key;
      break;

    case 10:
      inv_userid->reason = GPGME_Key_Not_Trusted;
      break;
    }

  while (*tail == ' ')
    tail++;
  if (*tail)
    {
      inv_userid->id = strdup (tail);
      if (!inv_userid->id)
	{
	  free (inv_userid);
	  return GPGME_Out_Of_Core;
	}
    }
  else
    inv_userid->id = NULL;

  *userid = inv_userid;
  return 0;
}