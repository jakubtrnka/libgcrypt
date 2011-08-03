/* cipher-cfb.c  - Generic CFB mode implementation
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003
 *               2005, 2007, 2008, 2009, 2011 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "cipher.h"
#include "ath.h"
#include "./cipher-internal.h"


gcry_err_code_t
_gcry_cipher_cfb_encrypt (gcry_cipher_hd_t c,
                          unsigned char *outbuf, unsigned int outbuflen,
                          const unsigned char *inbuf, unsigned int inbuflen)
{
  unsigned char *ivp;
  size_t blocksize = c->cipher->blocksize;
  size_t blocksize_x_2 = blocksize + blocksize;

  if (outbuflen < inbuflen)
    return GPG_ERR_BUFFER_TOO_SHORT;

  if ( inbuflen <= c->unused )
    {
      /* Short enough to be encoded by the remaining XOR mask. */
      /* XOR the input with the IV and store input into IV. */
      for (ivp=c->u_iv.iv+c->cipher->blocksize - c->unused;
           inbuflen;
           inbuflen--, c->unused-- )
        *outbuf++ = (*ivp++ ^= *inbuf++);
      return 0;
    }

  if ( c->unused )
    {
      /* XOR the input with the IV and store input into IV */
      inbuflen -= c->unused;
      for(ivp=c->u_iv.iv+blocksize - c->unused; c->unused; c->unused-- )
        *outbuf++ = (*ivp++ ^= *inbuf++);
    }

  /* Now we can process complete blocks.  We use a loop as long as we
     have at least 2 blocks and use conditions for the rest.  This
     also allows to use a bulk encryption function if available.  */
  if (inbuflen >= blocksize_x_2 && c->bulk.cfb_enc)
    {
      unsigned int nblocks = inbuflen / blocksize;
      c->bulk.cfb_enc (&c->context.c, c->u_iv.iv, outbuf, inbuf, nblocks);
      outbuf += nblocks * blocksize;
      inbuf  += nblocks * blocksize;
      inbuflen -= nblocks * blocksize;
    }
  else
    {
      while ( inbuflen >= blocksize_x_2 )
        {
          int i;
          /* Encrypt the IV. */
          c->cipher->encrypt ( &c->context.c, c->u_iv.iv, c->u_iv.iv );
          /* XOR the input with the IV and store input into IV.  */
          for(ivp=c->u_iv.iv,i=0; i < blocksize; i++ )
            *outbuf++ = (*ivp++ ^= *inbuf++);
          inbuflen -= blocksize;
        }
    }

  if ( inbuflen >= blocksize )
    {
      int i;
      /* Save the current IV and then encrypt the IV. */
      memcpy( c->lastiv, c->u_iv.iv, blocksize );
      c->cipher->encrypt ( &c->context.c, c->u_iv.iv, c->u_iv.iv );
      /* XOR the input with the IV and store input into IV */
      for(ivp=c->u_iv.iv,i=0; i < blocksize; i++ )
        *outbuf++ = (*ivp++ ^= *inbuf++);
      inbuflen -= blocksize;
    }
  if ( inbuflen )
    {
      /* Save the current IV and then encrypt the IV. */
      memcpy( c->lastiv, c->u_iv.iv, blocksize );
      c->cipher->encrypt ( &c->context.c, c->u_iv.iv, c->u_iv.iv );
      c->unused = blocksize;
      /* Apply the XOR. */
      c->unused -= inbuflen;
      for(ivp=c->u_iv.iv; inbuflen; inbuflen-- )
        *outbuf++ = (*ivp++ ^= *inbuf++);
    }
  return 0;
}


gcry_err_code_t
_gcry_cipher_cfb_decrypt (gcry_cipher_hd_t c,
                          unsigned char *outbuf, unsigned int outbuflen,
                          const unsigned char *inbuf, unsigned int inbuflen)
{
  unsigned char *ivp;
  unsigned long temp;
  int i;
  size_t blocksize = c->cipher->blocksize;
  size_t blocksize_x_2 = blocksize + blocksize;

  if (outbuflen < inbuflen)
    return GPG_ERR_BUFFER_TOO_SHORT;

  if (inbuflen <= c->unused)
    {
      /* Short enough to be encoded by the remaining XOR mask. */
      /* XOR the input with the IV and store input into IV. */
      for (ivp=c->u_iv.iv+blocksize - c->unused;
           inbuflen;
           inbuflen--, c->unused--)
        {
          temp = *inbuf++;
          *outbuf++ = *ivp ^ temp;
          *ivp++ = temp;
        }
      return 0;
    }

  if (c->unused)
    {
      /* XOR the input with the IV and store input into IV. */
      inbuflen -= c->unused;
      for (ivp=c->u_iv.iv+blocksize - c->unused; c->unused; c->unused-- )
        {
          temp = *inbuf++;
          *outbuf++ = *ivp ^ temp;
          *ivp++ = temp;
        }
    }

  /* Now we can process complete blocks.  We use a loop as long as we
     have at least 2 blocks and use conditions for the rest.  This
     also allows to use a bulk encryption function if available.  */
  if (inbuflen >= blocksize_x_2 && c->bulk.cfb_dec)
    {
      unsigned int nblocks = inbuflen / blocksize;
      c->bulk.cfb_dec (&c->context.c, c->u_iv.iv, outbuf, inbuf, nblocks);
      outbuf += nblocks * blocksize;
      inbuf  += nblocks * blocksize;
      inbuflen -= nblocks * blocksize;
    }
  else
    {
      while (inbuflen >= blocksize_x_2 )
        {
          /* Encrypt the IV. */
          c->cipher->encrypt ( &c->context.c, c->u_iv.iv, c->u_iv.iv );
          /* XOR the input with the IV and store input into IV. */
          for (ivp=c->u_iv.iv,i=0; i < blocksize; i++ )
            {
              temp = *inbuf++;
              *outbuf++ = *ivp ^ temp;
              *ivp++ = temp;
            }
          inbuflen -= blocksize;
        }
    }

  if (inbuflen >= blocksize )
    {
      /* Save the current IV and then encrypt the IV. */
      memcpy ( c->lastiv, c->u_iv.iv, blocksize);
      c->cipher->encrypt ( &c->context.c, c->u_iv.iv, c->u_iv.iv );
      /* XOR the input with the IV and store input into IV */
      for (ivp=c->u_iv.iv,i=0; i < blocksize; i++ )
        {
          temp = *inbuf++;
          *outbuf++ = *ivp ^ temp;
          *ivp++ = temp;
        }
      inbuflen -= blocksize;
    }

  if (inbuflen)
    {
      /* Save the current IV and then encrypt the IV. */
      memcpy ( c->lastiv, c->u_iv.iv, blocksize );
      c->cipher->encrypt ( &c->context.c, c->u_iv.iv, c->u_iv.iv );
      c->unused = blocksize;
      /* Apply the XOR. */
      c->unused -= inbuflen;
      for (ivp=c->u_iv.iv; inbuflen; inbuflen-- )
        {
          temp = *inbuf++;
          *outbuf++ = *ivp ^ temp;
          *ivp++ = temp;
        }
    }
  return 0;
}
