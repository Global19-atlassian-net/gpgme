/* sign.c - Signing function.
   Copyright (C) 2000 Werner Koch (dd9jn)
   Copyright (C) 2001, 2002, 2003 g10 Code GmbH

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
#include <string.h>
#include <errno.h>

#include "gpgme.h"
#include "context.h"
#include "ops.h"


typedef struct
{
  struct _gpgme_op_sign_result result;

  /* A pointer to the next pointer of the last invalid signer in
     the list.  This makes appending new invalid signers painless
     while preserving the order.  */
  gpgme_invalid_user_id_t *last_signer_p;

  /* Likewise for signature information.  */
  gpgme_new_signature_t *last_sig_p;
} *op_data_t;


static void
release_op_data (void *hook)
{
  op_data_t opd = (op_data_t) hook;
  gpgme_invalid_user_id_t invalid_signer = opd->result.invalid_signers;
  gpgme_new_signature_t sig = opd->result.signatures;

  while (invalid_signer)
    {
      gpgme_invalid_user_id_t next = invalid_signer->next;
      free (invalid_signer->id);
      free (invalid_signer);
      invalid_signer = next;
    }

  while (sig)
    {
      gpgme_new_signature_t next = sig->next;
      free (sig->fpr);
      free (sig);
      sig = next;
    }
}


gpgme_sign_result_t
gpgme_op_sign_result (gpgme_ctx_t ctx)
{
  op_data_t opd;
  gpgme_error_t err;

  err = _gpgme_op_data_lookup (ctx, OPDATA_SIGN, (void **) &opd, -1, NULL);
  if (err || !opd)
    return NULL;

  return &opd->result;
}


static gpgme_error_t
parse_sig_created (char *args, gpgme_new_signature_t *sigp)
{
  gpgme_new_signature_t sig;
  char *tail;

  sig = malloc (sizeof (*sig));
  if (!sig)
    return GPGME_Out_Of_Core;

  sig->next = NULL;
  switch (*args)
    {
    case 'S':
      sig->type = GPGME_SIG_MODE_NORMAL;
      break;

    case 'D':
      sig->type = GPGME_SIG_MODE_DETACH;
      break;

    case 'C':
      sig->type = GPGME_SIG_MODE_CLEAR;
      break;

    default:
      /* The backend engine is not behaving.  */
      free (sig);
      return GPGME_General_Error;
    }

  args++;
  if (*args != ' ')
    {
      free (sig);
      return GPGME_General_Error;
    }

  errno = 0;
  sig->pubkey_algo = strtol (args, &tail, 0);
  if (errno || args == tail || *tail != ' ')
    {
      /* The crypto backend does not behave.  */
      free (sig);
      return GPGME_General_Error;
    }
  args = tail;

  sig->hash_algo = strtol (args, &tail, 0);
  if (errno || args == tail || *tail != ' ')
    {
      /* The crypto backend does not behave.  */
      free (sig);
      return GPGME_General_Error;
    }
  args = tail;

  sig->class = strtol (args, &tail, 0);
  if (errno || args == tail || *tail != ' ')
    {
      /* The crypto backend does not behave.  */
      free (sig);
      return GPGME_General_Error;
    }
  args = tail;

  sig->timestamp = strtol (args, &tail, 0);
  if (errno || args == tail || *tail != ' ')
    {
      /* The crypto backend does not behave.  */
      free (sig);
      return GPGME_General_Error;
    }
  args = tail;
  while (*args == ' ')
    args++;

  if (!*args)
    {
      /* The crypto backend does not behave.  */
      free (sig);
      return GPGME_General_Error;
    }

  tail = strchr (args, ' ');
  if (tail)
    *tail = '\0';

  sig->fpr = strdup (args);
  if (!sig->fpr)
    {
      free (sig);
      return GPGME_Out_Of_Core;
    }
  *sigp = sig;
  return 0;
}


gpgme_error_t
_gpgme_sign_status_handler (void *priv, gpgme_status_code_t code, char *args)
{
  gpgme_ctx_t ctx = (gpgme_ctx_t) priv;
  gpgme_error_t err;
  op_data_t opd;

  err = _gpgme_passphrase_status_handler (priv, code, args);
  if (err)
    return err;

  err = _gpgme_op_data_lookup (ctx, OPDATA_SIGN, (void **) &opd, -1, NULL);
  if (err)
    return err;

  switch (code)
    {
    case GPGME_STATUS_SIG_CREATED:
      err = parse_sig_created (args, opd->last_sig_p);
      if (err)
	return err;

      opd->last_sig_p = &(*opd->last_sig_p)->next;
      break;

    case GPGME_STATUS_INV_RECP:
      err = _gpgme_parse_inv_userid (args, opd->last_signer_p);
      if (err)
	return err;

      opd->last_signer_p = &(*opd->last_signer_p)->next;
      break;

    case GPGME_STATUS_EOF:
      if (opd->result.invalid_signers)
	return GPGME_Invalid_UserID;
      break;

    default:
      break;
    }
  return err;
}


static gpgme_error_t
sign_status_handler (void *priv, gpgme_status_code_t code, char *args)
{
  return _gpgme_progress_status_handler (priv, code, args)
    || _gpgme_sign_status_handler (priv, code, args);
}


gpgme_error_t
_gpgme_op_sign_init_result (gpgme_ctx_t ctx)
{
  gpgme_error_t err;
  op_data_t opd;

  err = _gpgme_op_data_lookup (ctx, OPDATA_SIGN, (void **) &opd,
			       sizeof (*opd), release_op_data);
  if (err)
    return err;
  opd->last_signer_p = &opd->result.invalid_signers;
  opd->last_sig_p = &opd->result.signatures;
  return 0;
}


static gpgme_error_t
sign_start (gpgme_ctx_t ctx, int synchronous, gpgme_data_t plain,
	    gpgme_data_t sig, gpgme_sig_mode_t mode)
{
  gpgme_error_t err;

  err = _gpgme_op_reset (ctx, synchronous);
  if (err)
    return err;

  err = _gpgme_op_sign_init_result (ctx);
  if (err)
    return err;

  if (mode != GPGME_SIG_MODE_NORMAL && mode != GPGME_SIG_MODE_DETACH
      && mode != GPGME_SIG_MODE_CLEAR)
    return GPGME_Invalid_Value;

  if (!plain)
    return GPGME_No_Data;
  if (!sig)
    return GPGME_Invalid_Value;

  if (ctx->passphrase_cb)
    {
      err = _gpgme_engine_set_command_handler
	(ctx->engine, _gpgme_passphrase_command_handler, ctx, NULL);
      if (err)
	return err;
    }

  _gpgme_engine_set_status_handler (ctx->engine, sign_status_handler,
				    ctx);

  return _gpgme_engine_op_sign (ctx->engine, plain, sig, mode, ctx->use_armor,
				ctx->use_textmode, ctx->include_certs,
				ctx /* FIXME */);
}


/* Sign the plaintext PLAIN and store the signature in SIG.  */
gpgme_error_t
gpgme_op_sign_start (gpgme_ctx_t ctx, gpgme_data_t plain, gpgme_data_t sig,
		     gpgme_sig_mode_t mode)
{
  return sign_start (ctx, 0, plain, sig, mode);
}


/* Sign the plaintext PLAIN and store the signature in SIG.  */
gpgme_error_t
gpgme_op_sign (gpgme_ctx_t ctx, gpgme_data_t plain, gpgme_data_t sig,
	       gpgme_sig_mode_t mode)
{
  gpgme_error_t err = sign_start (ctx, 1, plain, sig, mode);
  if (!err)
    err = _gpgme_wait_one (ctx);
  return err;
}