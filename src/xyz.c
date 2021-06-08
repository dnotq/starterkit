/**
 * @file xyz.c
 * @date Apr 30, 2021
 * @author Matthew Hagerty
 */


#include "xyz.h"


/**
 * Initialize a meta data type.
 *
 * If the type requires malloc allocation
 *
 *
 * @param mt
 * @param type
 * @param alloc
 *
 * @return
 */
s32 xyz_meta_init(xyz_meta *mt, u32 type, u32 alloc, u32 dim, void *value)
{
   s32 rtn = XYZ_ERR;

   switch ( type )
   {
   case XYZ_META_T_ASCII_CHAR :    ///< ASCII padded character.
   case XYZ_META_T_ASCII_VARCHAR : ///< ASCII variable character.
   case XYZ_META_T_UTF8_CHAR :     ///< UTF8 padded character.
   case XYZ_META_T_UTF8_VARCHAR :  ///< UTF8 variable character.

      mt->format = XYZ_META_F_POINTER;
      mt->type = type;

      if ( alloc == XYZ_META_P_STATIC )
      {
         if ( value != NULL ) {
            mt->buf.vp = value;
         } else {
            // TODO error.
            break;
         }
      }

      else if ( alloc == XYZ_META_P_DYNAMIC )
      {

      }


      rtn = XYZ_OK;
      break;

   default :
      // TODO unknown.
      break;
   }

   return rtn;
}
// xyz_meta_init()
