/* data-compat.c - Compatibility interfaces for data objects.
   Copyright (C) 2002 g10 Code GmbH
 
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

#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "data.h"
#include "util.h"


/* Create a new data buffer filled with LENGTH bytes starting from
   OFFSET within the file FNAME or stream STREAM (exactly one must be
   non-zero).  */
gpgme_error_t
gpgme_data_new_from_filepart (gpgme_data_t *dh, const char *fname,
			      FILE *stream, off_t offset, size_t length)
{
  gpgme_error_t err;
  char *buf = NULL;

  if (stream && fname)
    return GPGME_Invalid_Value;

  if (fname)
    stream = fopen (fname, "rb");
  if (!stream)
    return GPGME_File_Error;

  if (fseek (stream, offset, SEEK_SET))
    goto ferr;

  buf = malloc (length);
  if (!buf)
    goto ferr;

  while (fread (buf, length, 1, stream) < 1
	 && ferror (stream) && errno == EINTR);
  if (ferror (stream))
    {
      if (buf)
	free (buf);
      goto ferr;
    }

  if (fname)
    fclose (stream);

  err = gpgme_data_new (dh);
  if (err)
    {
      if (buf)
	free (buf);
      return err;
    }

  (*dh)->data.mem.buffer = buf;
  (*dh)->data.mem.size = length;
  (*dh)->data.mem.length = length;
  return 0;

 ferr:
  {
    int saved_errno = errno;
    if (fname)
      fclose (stream);
    errno = saved_errno;
    return GPGME_File_Error;
  }
}


/* Create a new data buffer filled with the content of file FNAME.
   COPY must be non-zero (delayed reads are not supported yet).  */
gpgme_error_t
gpgme_data_new_from_file (gpgme_data_t *dh, const char *fname, int copy)
{
  struct stat statbuf;

  if (!fname || !copy)
    return GPGME_Invalid_Value;

  if (stat (fname, &statbuf) < 0)
    return GPGME_File_Error;

  return gpgme_data_new_from_filepart (dh, fname, NULL, 0, statbuf.st_size);
}


static int
gpgme_error_to_errno (gpgme_error_t err)
{
  switch (err)
    {
    case GPGME_EOF:
      return 0;
    case GPGME_Out_Of_Core:
      errno = ENOMEM;
      return -1;
    case GPGME_Invalid_Value:
      errno = EINVAL;
      return -1;
    case GPGME_Not_Implemented:
      errno = EOPNOTSUPP;
      return -1;
    default:
      /* XXX Yeah, well.  */
      errno = EINVAL;
      return -1;
    }
}

static ssize_t
old_user_read (gpgme_data_t dh, void *buffer, size_t size)
{
  size_t amt;
  gpgme_error_t err = (*dh->data.old_user.cb) (dh->data.old_user.handle,
					       buffer, size, &amt);
  if (err)
    return gpgme_error_to_errno (err);
  return amt;
}


static off_t
old_user_seek (gpgme_data_t dh, off_t offset, int whence)
{
  gpgme_error_t err;
  if (whence != SEEK_SET || offset)
    return EINVAL;
  err = (*dh->data.old_user.cb) (dh->data.old_user.handle, NULL, 0, NULL);
  if (err)
    return gpgme_error_to_errno (err);
  return 0;
}


static struct _gpgme_data_cbs old_user_cbs =
  {
    old_user_read,
    NULL,
    old_user_seek,
    NULL
  };


/* Create a new data buffer which retrieves the data from the callback
   function READ_CB.  */
gpgme_error_t
gpgme_data_new_with_read_cb (gpgme_data_t *dh,
                             int (*read_cb) (void *, char *, size_t, size_t *),
                             void *read_cb_value)
{
  gpgme_error_t err = _gpgme_data_new (dh, &old_user_cbs);
  if (err)
    return err;

  (*dh)->data.old_user.cb = read_cb;
  (*dh)->data.old_user.handle = read_cb_value;
  return 0;
}


gpgme_error_t
gpgme_data_rewind (gpgme_data_t dh)
{
  return (gpgme_data_seek (dh, 0, SEEK_SET) == -1)
    ? GPGME_File_Error : 0;
}