/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

/* C++ files must define __STDC_FORMAT_MACROS in order to get PRIu64 */
#define __STDC_FORMAT_MACROS 
#include <inttypes.h>

#include <NdbApi.hpp>
#include "decimal_utils.hpp"

#include <memcached/util.h>
#include <memcached/extension_loggers.h>

#include "DataTypeHandler.h"
#include "debug.h"
#include "int3korr.h"
   

extern EXTENSION_LOGGER_DESCRIPTOR *logger;

#define DECODE_ARGS const NdbDictionary::Column *, char * &, const void * const
#define SFDLEN_ARGS const NdbDictionary::Column *, const void * const
#define ENCODE_ARGS const NdbDictionary::Column *, size_t, const char *, void * const

typedef int    impl_readFromNdb(DECODE_ARGS);
typedef size_t impl_getStringifiedLength(SFDLEN_ARGS);
typedef int    impl_writeToNdb(ENCODE_ARGS);

/* Implementations for NumericHandlers */
template<typename INTTYPE> int dth_read32(Int32 &, const void * const);
template<typename INTTYPE> int dth_write32(Int32, void * const);
int dth_read32_year(Int32 &, const void * const);
int dth_write32_year(Int32, void * const);
int dth_read32_medium(Int32 &, const void * const);
int dth_write32_medium(Int32, void * const);
int dth_read32_medium_unsigned(Int32 &, const void * const);
int dth_write32_medium_unsigned(Int32, void * const);


/* Implementations for readFromNdb() */
impl_readFromNdb dth_decode_unsupported; 
impl_readFromNdb dth_decode_varchar; 
impl_readFromNdb dth_decode_longvarchar; 
impl_readFromNdb dth_decode_char; 
impl_readFromNdb dth_decode_enum; 
impl_readFromNdb dth_decode_tinyint; 
impl_readFromNdb dth_decode_smallint; 
impl_readFromNdb dth_decode_mediumint; 
impl_readFromNdb dth_decode_int; 
impl_readFromNdb dth_decode_bigint; 
impl_readFromNdb dth_decode_tiny_unsigned; 
impl_readFromNdb dth_decode_small_unsigned; 
impl_readFromNdb dth_decode_medium_unsigned; 
impl_readFromNdb dth_decode_unsigned; 
impl_readFromNdb dth_decode_ubigint; 
impl_readFromNdb dth_decode_year; 
impl_readFromNdb dth_decode_date; 
impl_readFromNdb dth_decode_time; 
impl_readFromNdb dth_decode_datetime; 
impl_readFromNdb dth_decode_float; 
impl_readFromNdb dth_decode_double; 
impl_readFromNdb dth_decode_decimal; 


/* Implementations for impl_getStringifiedLength() */
// declaring it this way causes gcc to crash:
// template<typename INTTYPE> impl_getStringifiedLength dth_length_s;
template<typename INTTYPE> size_t dth_length_s(SFDLEN_ARGS);
template<typename INTTYPE> size_t dth_length_u(SFDLEN_ARGS);
impl_getStringifiedLength dth_length_unsupported;
impl_getStringifiedLength dth_length_varchar;
impl_getStringifiedLength dth_length_longvarchar;
impl_getStringifiedLength dth_length_char;
impl_getStringifiedLength dth_length_enum;
impl_getStringifiedLength dth_length_mediumint;
impl_getStringifiedLength dth_length_medium_unsigned;
impl_getStringifiedLength dth_length_year;
impl_getStringifiedLength dth_length_date;
impl_getStringifiedLength dth_length_time;
impl_getStringifiedLength dth_length_datetime;
impl_getStringifiedLength dth_length_float;
impl_getStringifiedLength dth_length_double;
impl_getStringifiedLength dth_length_decimal;


/* Implementations for writeToNdb() */
impl_writeToNdb dth_encode_unsupported;
impl_writeToNdb dth_encode_varchar;
impl_writeToNdb dth_encode_longvarchar;
impl_writeToNdb dth_encode_char;
impl_writeToNdb dth_encode_enum;
impl_writeToNdb dth_encode_tinyint;
impl_writeToNdb dth_encode_smallint;
impl_writeToNdb dth_encode_mediumint;
impl_writeToNdb dth_encode_int;
impl_writeToNdb dth_encode_bigint;
impl_writeToNdb dth_encode_tiny_unsigned;
impl_writeToNdb dth_encode_small_unsigned;
impl_writeToNdb dth_encode_medium_unsigned;
impl_writeToNdb dth_encode_unsigned;
impl_writeToNdb dth_encode_ubigint;
impl_writeToNdb dth_encode_year;
impl_writeToNdb dth_encode_date;
impl_writeToNdb dth_encode_time;
impl_writeToNdb dth_encode_datetime;
template<typename T> int dth_encode_fp(ENCODE_ARGS);
impl_writeToNdb dth_encode_decimal;

/* Native Numeric Handlers */
NumericHandler dth_native_int8   = {1, dth_read32<Int8>,  dth_write32<Int8>  };
NumericHandler dth_native_int16  = {2, dth_read32<Int16>, dth_write32<Int16> };
NumericHandler dth_native_int32  = {4, dth_read32<Int32>, dth_write32<Int32> };
NumericHandler dth_native_uint8  = {1, dth_read32<Uint8>,  dth_write32<Uint8> };
NumericHandler dth_native_uint16 = {2, dth_read32<Uint16>, dth_write32<Uint16>};
NumericHandler dth_native_uint32 = {4, dth_read32<Uint32>, dth_write32<Uint32>};
NumericHandler dth_native_year   = {1, dth_read32_year, dth_write32_year };
NumericHandler dth_native_medium = {1, dth_read32_medium, dth_write32_medium };
NumericHandler dth_native_medium_unsigned = 
                 { 1, dth_read32_medium_unsigned, dth_write32_medium_unsigned };
NumericHandler dth_native_64     = {8, NULL, NULL };                 


/***** Singleton Handlers *****/
DataTypeHandler Handler_unsupported = {
  dth_decode_unsupported,
  dth_length_unsupported,
  dth_encode_unsupported,
  0,
  false
};

DataTypeHandler Handler_Varchar =  {
  dth_decode_varchar,      // readFromNdb()
  dth_length_varchar,      // getStringifiedLength()
  dth_encode_varchar,      // writeToNdb()
  0,                       // native_handler
  2                        // contains_string + 1-byte length
};

DataTypeHandler Handler_LongVarchar = {
  dth_decode_longvarchar,
  dth_length_longvarchar,
  dth_encode_longvarchar,
  0,
  3                       // contains string + 2 length bytes
};

DataTypeHandler Handler_Char =  {
  dth_decode_char,        // readFromNdb()
  dth_length_char,        // getStringifiedLength()
  dth_encode_char,        // writeToNdb()
  0,                      // native_handler
  1                       // contains_string (no length bytes)
};

DataTypeHandler Handler_enum = {   /* NDB sees ENUM columns as CHAR(1) */
  dth_decode_enum,
  dth_length_enum,
  dth_encode_enum,
  & dth_native_int8,
  false
};

DataTypeHandler Handler_Tinyint = {
  dth_decode_tinyint,
  dth_length_s<Int8>,
  dth_encode_tinyint,
  & dth_native_int8,
  false
};

DataTypeHandler Handler_Smallint = {
  dth_decode_smallint,
  dth_length_s<Int16>,
  dth_encode_smallint,
  & dth_native_int16,
  false
};

DataTypeHandler Handler_Mediumint = {
  dth_decode_mediumint,
  dth_length_mediumint,
  dth_encode_mediumint,
  & dth_native_medium,
  false
};

DataTypeHandler Handler_Int = {
  dth_decode_int,
  dth_length_s<Int32>,
  dth_encode_int,
  & dth_native_int32,
  false
};

DataTypeHandler Handler_Bigint = {
  dth_decode_bigint,
  dth_length_s<Int64>,
  dth_encode_bigint,
  & dth_native_64, 
  false
};

DataTypeHandler Handler_Tiny_Unsigned = {
  dth_decode_tiny_unsigned,
  dth_length_u<Uint8>,
  dth_encode_tiny_unsigned,
  & dth_native_uint8,
  false
};

DataTypeHandler Handler_Small_Unsigned = {
  dth_decode_small_unsigned,
  dth_length_u<Uint16>,
  dth_encode_small_unsigned,
  & dth_native_uint16,
  false
};

DataTypeHandler Handler_Medium_Unsigned = {
  dth_decode_medium_unsigned,
  dth_length_medium_unsigned,
  dth_encode_medium_unsigned,
  & dth_native_medium_unsigned,
  false
};

DataTypeHandler Handler_Unsigned = {
  dth_decode_unsigned,
  dth_length_u<Uint32>,
  dth_encode_unsigned,
  & dth_native_uint32,
  false
};

DataTypeHandler Handler_BigIntUnsigned = {
  dth_decode_ubigint,
  dth_length_u<Uint64>,
  dth_encode_ubigint,
  & dth_native_64,   /* also see setUint64Value() in Record.cc */
  false
};

DataTypeHandler Handler_Year = {
  dth_decode_year,
  dth_length_year,
  dth_encode_year,
  & dth_native_year, 
  false
};

DataTypeHandler Handler_Date = {
  dth_decode_date,
  dth_length_date,
  dth_encode_date,
  0, 
  false
};

DataTypeHandler Handler_Time = {
  dth_decode_time,
  dth_length_time,
  dth_encode_time,
  0, 
  false
};

DataTypeHandler Handler_Datetime = {
  dth_decode_datetime,
  dth_length_datetime,
  dth_encode_datetime,
  0, 
  false
};

DataTypeHandler Handler_Float = {
  dth_decode_float,
  dth_length_float,
  dth_encode_fp<float>,
  0, 
  false
};

DataTypeHandler Handler_Double = {
  dth_decode_double,
  dth_length_double,
  dth_encode_fp<double>,
  0, 
  false
};

DataTypeHandler Handler_Decimal = {
  dth_decode_decimal,
  dth_length_decimal,
  dth_encode_decimal,
  0, 
  false
};


/*
 * getDataTypeHandlerForColumn() 
 */
DataTypeHandler * getDataTypeHandlerForColumn(const NdbDictionary::Column *col) {
  switch(col->getType()) {
    case NdbDictionary::Column::Varchar:      
    case NdbDictionary::Column::Varbinary:      
      return & Handler_Varchar;

    case NdbDictionary::Column::Longvarchar:
    case NdbDictionary::Column::Longvarbinary:
      return & Handler_LongVarchar;

    case NdbDictionary::Column::Int:
      return & Handler_Int;

    case NdbDictionary::Column::Unsigned:
    case NdbDictionary::Column::Timestamp:
      return & Handler_Unsigned;
    
    case NdbDictionary::Column::Bigint:
      return & Handler_Bigint;

    case NdbDictionary::Column::Bigunsigned:
      return & Handler_BigIntUnsigned;
      
    case NdbDictionary::Column::Char:
      if(col->getLength() == 1) return & Handler_enum;
      else return & Handler_Char;

    case NdbDictionary::Column::Tinyint:
      return & Handler_Tinyint;

    case NdbDictionary::Column::Tinyunsigned:
      return & Handler_Tiny_Unsigned;

    case NdbDictionary::Column::Smallint:
      return & Handler_Smallint;

    case NdbDictionary::Column::Smallunsigned:
      return & Handler_Small_Unsigned;
      
    case NdbDictionary::Column::Mediumint:
      return & Handler_Mediumint;

    case NdbDictionary::Column::Mediumunsigned:
      return & Handler_Medium_Unsigned;
    
    case NdbDictionary::Column::Year:
      return & Handler_Year;

    case NdbDictionary::Column::Date:
      return & Handler_Date;
      
    case NdbDictionary::Column::Time:
      return & Handler_Time;

    case NdbDictionary::Column::Datetime:
      return & Handler_Datetime;
      
    case NdbDictionary::Column::Float:
      return & Handler_Float;

    case NdbDictionary::Column::Double:
      return & Handler_Double;

    case NdbDictionary::Column::Decimal:
    case NdbDictionary::Column::Decimalunsigned:
      return & Handler_Decimal;

    default:
      return & Handler_unsupported;
  }
}

/* 
 * getColumnRecordSize() 
 */
size_t getColumnRecordSize(const NdbDictionary::Column *col) {
  switch(col->getType()) {
    case NdbDictionary::Column::Varchar:      
    case NdbDictionary::Column::Varbinary:
      return col->getLength() + 1;
    
    case NdbDictionary::Column::Longvarchar:
    case NdbDictionary::Column::Longvarbinary:    
      return col->getLength() + 2;

    case NdbDictionary::Column::Char:
    case NdbDictionary::Column::Binary:
      return col->getLength();

    default:
      return col->getSizeInBytes();
  }
}


/******************* IMPLEMENTATIONS *******************/
/***** UNSUPPORTED COLUMN TYPE ******/
int dth_decode_unsupported(const NdbDictionary::Column *col, char *&, const void *) {
  logger->log(LOG_WARNING, 0, "Unsupported column type: %s\n", col->getName());
  return DTH_NOT_SUPPORTED;
}

size_t dth_length_unsupported(const NdbDictionary::Column *, const void *) {
  return 0;
}

int dth_encode_unsupported(const NdbDictionary::Column *col, size_t,
                           const char *, void *) {
  logger->log(LOG_WARNING, 0, "Unsupported column type: %s\n", col->getName());
  return DTH_NOT_SUPPORTED;
}
      

/***** VARCHAR *****/
int dth_decode_varchar(const NdbDictionary::Column *col, 
                       char * &str, const void *buf) {    
  size_t len = dth_length_varchar(col, buf);
  str = ((char *) buf) + 1;
  return len;
}


size_t dth_length_varchar(const NdbDictionary::Column *col, const void *buf) {
  /* Return the actual length of the value string */
  uint8_t * length_byte = (uint8_t *) buf;  
  return (size_t) (*length_byte);
}


int dth_encode_varchar(const NdbDictionary::Column *col, 
                       size_t len, const char *str, void *buf) {
  uint8_t * length_byte = (uint8_t *) buf;
  char *char_buffer = ((char *) buf) + 1;
    
  if(len > col->getLength())
    return DTH_VALUE_TOO_LONG;
  
  /* Set the length byte */
  *length_byte = (uint8_t) len;
  
  /* Copy string value into buffer */
  strncpy(char_buffer, str, len);

  return len;
}



/***** LONGVARCHAR *****/
int dth_decode_longvarchar(const NdbDictionary::Column *col, 
                           char * &str, const void *buf) {  
  size_t len = dth_length_longvarchar(col, buf);
  str = ((char *) buf) + 2;
  return len;
}

size_t dth_length_longvarchar(const NdbDictionary::Column *col, const void *buf) {
  /* Return the actual length of the value string */
  uint8_t * length_byte_1 = (uint8_t *) buf;
  uint8_t * length_byte_2 = ((uint8_t *) buf) + 1;
  
  return (size_t) ( *length_byte_1 + (*length_byte_2 << 8));
}

int dth_encode_longvarchar(const NdbDictionary::Column *col, size_t len, 
                           const char *str, void *buf) {
  char *cbuf = ((char *) buf);
  char *dest = cbuf + 2;
  unsigned short total_len = len;
  const unsigned short short_lo = 255;
  const unsigned short short_hi = 65535 ^ 255; 
  
  if(total_len > col->getLength())
    return DTH_VALUE_TOO_LONG;
  
  /* Set the length bytes */
  * cbuf     = (char) (total_len & short_lo);
  * (cbuf+1) = (char) ((total_len & short_hi) >> 8);
  
  /* Copy string value into buffer */
  strncpy(dest, str, len);
  
  return len;
}


/***** CHAR *****/
int dth_decode_char(const NdbDictionary::Column *col,
                    char * &str, const void *buf) {
  str = (char *) buf;
  return col->getLength();  // value is padded with spaces
}

size_t dth_length_char(const NdbDictionary::Column *col, const void *buf) {
  return col->getLength();
}

int dth_encode_char(const NdbDictionary::Column *col, size_t len,
                    const char *str, void *buf) {
  char *cbuf = ((char *) buf);
  char *dest = cbuf;
  if(len > col->getLength()) 
    return DTH_VALUE_TOO_LONG;

  /* copy string into buffer */
  memcpy(dest, str, len);

  /* right-pad with spaces */
  for(char *s = dest+len ; len <= col->getLength() ; len++) {
    *(s++) = ' ';
  }

  return len;
}

/***** TEMPLATES, HELPERS, MACROS FOR NUMERIC TYPES *****/

/* Using separate templates for signed and unsigned types avoids 
   a compiler warning */

template<typename INTTYPE> size_t dth_length_s(const NdbDictionary::Column *,
                                               const void *buf) {
  LOAD_ALIGNED_DATA(INTTYPE, i, buf);
  size_t len = (i < 0) ? 2 : 1;  
  for( ; i > 0 ; len++) i = i / 10;
  return len;  
}

template<typename INTTYPE> size_t dth_length_u(const NdbDictionary::Column *,
                                               const void *buf) {
  LOAD_ALIGNED_DATA(INTTYPE, i, buf);
  size_t len = 1;
  for( ; i > 0 ; len++) i = i / 10;
  return len;  
}

/* read32: read the value from the buffer into an int32 */
template<typename INTTYPE> int dth_read32(Int32 &result, const void * const buf) {
  LOAD_ALIGNED_DATA(INTTYPE, i, buf);
  result = (Int32) i;
  return 1;
}

/* write32: write an int32 into the buffer */
template<typename INTTYPE> int dth_write32(Int32 value, void *buf) {
  STORE_ALIGNED_DATA(INTTYPE, value, buf);
  return 1;
}


/* Make a safe copy of the text representation of a number, discarding 
   any terminal junk characters that may be in the buffer */
#define MAKE_COPY_BUFFER(sz) \
  char copy_buff[sz]; \
  if(len >= sz) return DTH_VALUE_TOO_LONG; \
  strncpy(copy_buff, str, len); \
  copy_buff[len] = '\0';
  


/***** TINYINT *****/
int dth_decode_tinyint(const NdbDictionary::Column *col,
                       char * &str, const void *buf) {
  char i = * (char *) buf;
  return sprintf(str, "%d", (int) i);
}

int dth_encode_tinyint(const NdbDictionary::Column *col, size_t len,
                       const char *str, void *buf) {
  MAKE_COPY_BUFFER(8);
  int intval = 0;

  if((! safe_strtol(copy_buff, &intval)) || intval > 127 || intval < -128) {
      return DTH_NUMERIC_OVERFLOW;
  }
  
  *((Int8 *) buf) = (Int8) intval;
  return len;
}


/***** TINY UNSIGNED *****/
int dth_decode_tiny_unsigned(const NdbDictionary::Column *col,
                             char * &str, const void *buf) {
  Uint8 i = * (Uint8 *) buf;
  return sprintf(str, "%d", (int) i);
}

int dth_encode_tiny_unsigned(const NdbDictionary::Column *, size_t len, 
                             const char *str, void *buf) {
  MAKE_COPY_BUFFER(8);
  Uint32 intval = 0;
  
  if((! safe_strtoul(copy_buff, &intval)) || intval > 255) {
    return DTH_NUMERIC_OVERFLOW;
  }
  
  *((Uint8 *) buf) = (Uint8) intval;
  return len;
}



/***** SMALLINT ******/
int dth_decode_smallint(const NdbDictionary::Column *col,
                        char * &str, const void *buf) {  
  LOAD_ALIGNED_DATA(Int16, shortval, buf);
  return sprintf(str, "%hd", shortval);
}

int dth_encode_smallint(const NdbDictionary::Column *col, size_t len, 
                        const char *str, void *buf) {
  MAKE_COPY_BUFFER(8);
  int intval = 0;
  
  if((! safe_strtol(copy_buff, &intval)) || intval > 32767 || intval < -32768) {
    return DTH_NUMERIC_OVERFLOW;
  }
  STORE_ALIGNED_DATA(Int16, intval, buf);
  
  return len;
}


/***** SMALL UNSIGNED ******/
int dth_decode_small_unsigned(const NdbDictionary::Column *col,
                              char * &str, const void *buf) {
  LOAD_ALIGNED_DATA(Uint16, shortval, buf);
  return sprintf(str, "%hu", shortval);
}

int dth_encode_small_unsigned(const NdbDictionary::Column *col, size_t len, 
                              const char *str, void *buf) {
  MAKE_COPY_BUFFER(8);
  Uint32 intval = 0;
  
  if((! safe_strtoul(copy_buff, &intval)) || intval > 65535) {
    return DTH_NUMERIC_OVERFLOW;
  }
  STORE_ALIGNED_DATA(Uint16, intval, buf);
  return len;
}


/***** MEDIUMINT ******/
int dth_decode_mediumint(const NdbDictionary::Column *col,
                         char * &str, const void *buf) {
  char * cbuf = (char *) buf;
  int i = sint3korr(cbuf);  
  return sprintf(str, "%d", i);
}

size_t dth_length_mediumint(const NdbDictionary::Column *col, const void *buf) {
  char * cbuf = (char *) buf;
  int i = sint3korr(cbuf);
  int len = ( i < 0) ? 2 : 1;  
  for( ; i > 0 ; len++) i = i / 10;
  return len;  
}

int dth_encode_mediumint(const NdbDictionary::Column *col, size_t len,
                         const char *str, void *buf) {
  int intval = 0;
  const int MAXVAL = 8388607;
  const int MINVAL = -8388608;  
  MAKE_COPY_BUFFER(16);
  
  if((! safe_strtol(copy_buff, &intval)) || intval > MAXVAL || intval < MINVAL) {
    return DTH_NUMERIC_OVERFLOW;
  }

  Int8 *cbuf = (Int8 *) buf;
  cbuf[0] = (Int8) (intval);
  cbuf[1] = (Int8) (intval >> 8);
  cbuf[2] = (Int8) (intval >> 16);

  return len;
}

int dth_read32_medium(Int32 &result, const void * const buf) {
  result = sint3korr((char *) buf);
  return 1;
}

int dth_write32_medium(Int32 value, void *buf) {
  Int8 *cbuf = (Int8 *) buf;
  cbuf[0] = (Int8) (value);
  cbuf[1] = (Int8) (value >> 8);
  cbuf[2] = (Int8) (value >> 16);
  return 1;
}


/***** MEDIUM UNSIGNED ******/
int dth_decode_medium_unsigned(const NdbDictionary::Column *,
                               char * &str, const void *buf) {
  char * cbuf = (char *) buf;
  unsigned int i = uint3korr(cbuf);
  return sprintf(str, "%u", i);
}

size_t dth_length_medium_unsigned(const NdbDictionary::Column *, const void *buf) {
  char * cbuf = (char *) buf;
  int i = uint3korr(cbuf);
  int len = 1;  
  for( ; i > 0 ; len++) i = i / 10;
  return len;  
}

int dth_encode_medium_unsigned(const NdbDictionary::Column *, size_t len,
                               const char *str, void *buf) {
  MAKE_COPY_BUFFER(16);
  Uint32 intval = 0;
  
  if((! safe_strtoul(copy_buff, &intval)) || intval > 16277215) {
    return DTH_NUMERIC_OVERFLOW;
  }
  
  Uint8 *cbuf = (Uint8 *) buf;
  cbuf[0] = (Uint8) (intval);
  cbuf[1] = (Uint8) (intval >> 8);
  cbuf[2] = (Uint8) (intval >> 16);
  
  return len;
}

int dth_read32_medium_unsigned(Int32 &result, const void * const buf) {
  result = uint3korr((char *) buf);
  return 1;
}

int dth_write32_medium_unsigned(Int32 value, void *buf) {
  Uint8 *cbuf = (Uint8 *) buf;
  cbuf[0] = (Uint8) (value);
  cbuf[1] = (Uint8) (value >> 8);
  cbuf[2] = (Uint8) (value >> 16);
  return 1;
}


/***** INT *****/
int dth_decode_int(const NdbDictionary::Column *col,
                   char * &str, const void *buf) {  
  LOAD_ALIGNED_DATA(int, i, buf);
  return sprintf(str, "%d", i);
}

int dth_encode_int(const NdbDictionary::Column *col, size_t len, 
                   const char *str, void *buf) {
  MAKE_COPY_BUFFER(32);
  int intval = 0;

  if(! safe_strtol(copy_buff, &intval))
    return DTH_NUMERIC_OVERFLOW;

  STORE_ALIGNED_DATA(int, intval, buf);
  return len;
}                                 


/***** INT UNSIGNED *****/
int dth_decode_unsigned(const NdbDictionary::Column *col,
                        char * &str, const void *buf) {  
  LOAD_ALIGNED_DATA(Uint32, i, buf)
  return sprintf(str, "%du", i);
}

int dth_encode_unsigned(const NdbDictionary::Column *col, size_t len,
                        const char *str, void *buf) {
  MAKE_COPY_BUFFER(32);
  Uint32 uintval = 0;
  
  if(! safe_strtoul(copy_buff, &uintval))
    return DTH_NUMERIC_OVERFLOW;

  STORE_ALIGNED_DATA(Uint32, uintval, buf);
  return len;
}                                 


/***** BIGINT *****/
int dth_decode_bigint(const NdbDictionary::Column *col,  
                      char * &str, const void *buf) {
  LOAD_ALIGNED_DATA(Int64, int64val, buf);
  return sprintf(str, "%"PRId64, int64val);
}

int dth_encode_bigint(const NdbDictionary::Column *col, size_t len,
                      const char *str, void *buf) {
  MAKE_COPY_BUFFER(32);
  int64_t int64val = 0;
  
  if(! safe_strtoll(copy_buff, &int64val)) 
    return DTH_NUMERIC_OVERFLOW;

  STORE_ALIGNED_DATA(int64_t, int64val, buf);
  return len;
}


/***** BIGINT UNSIGNED *****/
int dth_decode_ubigint(const NdbDictionary::Column *col,  
                       char * &str, const void *buf) {
  LOAD_ALIGNED_DATA(Uint64, uint64val, buf);
  return sprintf(str, "%"PRIu64, uint64val);
}

int dth_encode_ubigint(const NdbDictionary::Column *col, size_t len,
                       const char *str, void *buf) {
  MAKE_COPY_BUFFER(32);
  uint64_t uint64val = 0;

  if(! safe_strtoull(copy_buff, &uint64val))  
    return DTH_NUMERIC_OVERFLOW;

  STORE_ALIGNED_DATA(uint64_t, uint64val, buf);
  return len;
}                                 


/***** ENUM *****/
int dth_decode_enum(const NdbDictionary::Column *col,  
                    char * &str, const void *buf) {                                   
  *str = * (char *) buf;  
  return 1;
}

size_t dth_length_enum(const NdbDictionary::Column *col, const void *buf) {
  return 1;
}

int dth_encode_enum(const NdbDictionary::Column *col, size_t len,
                    const char *str, void *buf) {
  char *cbuf = (char *) buf;
  *cbuf = *str;  
  return 1;
}


/***** YEAR *****/
int dth_decode_year(const NdbDictionary::Column *, char * &str, const void *buf) {
  Uint8 i = * (Uint8 *) buf;
  int year = i + 1900;
  return sprintf(str, "%d", year);
}

size_t dth_length_year(const NdbDictionary::Column *col, const void *buf) {
  return 5;
}

int dth_encode_year(const NdbDictionary::Column *, size_t len, 
                    const char *str, void *buf) {
  MAKE_COPY_BUFFER(8);
  Uint32 intval = 0;
  
  if((! safe_strtoul(copy_buff, &intval)) || intval < 1900 || intval > 2155) {
    return DTH_NUMERIC_OVERFLOW;
  }
  
  intval -= 1900;
  *((Uint8 *) buf) = (Uint8) intval;
  return len;
}

int dth_read32_year(Int32 &result, const void * const buf) {
  Uint8 i = *((Uint8 *) buf);
  result = ((Int32) i) + 1900;
  return 1;
}

int dth_write32_year(Int32 value, void *buf) {
  if(value < 1900 || value > 2155) 
    return 0;
  Uint8 i = (Uint8) (value - 1900);
  * ((Uint8 *) buf) = i;
  return 1;
}


/***** DATE & TIME HELPERS *****/

typedef struct {
  unsigned int year, month, day, hour, minute, second, fraction;
  bool is_negative;
} time_helper;

inline void factor_HHMMSS(time_helper *tm, Int32 int_time) {
  if(int_time < 0) {
    tm->is_negative = true; int_time = - int_time;
  }
  tm->hour   = int_time/10000;
  tm->minute = int_time/100 % 100;
  tm->second = int_time % 100;  
}

inline void factor_YYYYMMDD(time_helper *tm, Int32 int_date) {
  tm->year  = int_date/10000 % 10000;
  tm->month = int_date/100 % 100;
  tm->day   = int_date % 100;  
}

class DateTime_CopyBuffer {
public:
  DateTime_CopyBuffer(size_t len, const char *str);
  const char *ptr;
  bool too_long;
private:
  char copy_buffer[64];
};

/* Make a safe copy of a supplied date, time, or datetime,
   insuring null termination and skipping separators 
*/
DateTime_CopyBuffer::DateTime_CopyBuffer(size_t len, const char *str) {
  const char *c = str;
  char * buf = copy_buffer;
  ptr = copy_buffer;
  
  too_long = ( len > 60);  
  if(! too_long) {
    if(*c == '-' || *c == '+') *buf++ = *c++;  // tolerate initial + or -

    for(register unsigned int i = 0 ; i < len && *c != 0 ; c++, i++ )
      if(isdigit(*c))
        *buf++ = *c; 
    *buf = 0;
  }
}


/***** DATE *****/

int dth_decode_date(const NdbDictionary::Column *, char * &str, const void *buf) {
  Int32 encoded_date;
  time_helper tm = { 0,0,0,0,0,0,0, false };

  /* Read the encoded date from the buffer */
  dth_read32_medium_unsigned(encoded_date, buf);

  /* Unpack the encoded date */
  tm.day   = (encoded_date & 31);  // five bits
  tm.month = (encoded_date >> 5 & 15); // four bits
  tm.year  = (encoded_date >> 9);
  
  return sprintf(str, "%04du-%02du-%02du",tm.year, tm.month, tm.day);
}

size_t dth_length_date(const NdbDictionary::Column *col, const void *buf) {
  return 12;
}

int dth_encode_date(const NdbDictionary::Column *, size_t len, 
                    const char *str, void *buf) {
  Int32  int_date;
  Uint32  encoded_date; 
  time_helper tm = { 0,0,0,0,0,0,0, false };

  /* Make a safe (null-terminated) copy */
  DateTime_CopyBuffer copybuff(len, str);
 
  /* Turn the safe copy into an int */
  if(copybuff.too_long) return DTH_VALUE_TOO_LONG;
  if(! safe_strtol(copybuff.ptr, &int_date)) return DTH_NUMERIC_OVERFLOW;

  /* Factor out the Year/Month/Day */
  factor_YYYYMMDD(& tm, int_date);

  /* Encode for MySQL */
  encoded_date = (tm.year << 9) | (tm.month << 5) | tm.day;

  /* Store the encoded value as an UNSIGNED MEDIUM */
  return dth_write32_medium_unsigned(encoded_date, (char *) buf);
}


/***** TIME *****/

int dth_decode_time(const NdbDictionary::Column *, char * &str, const void *buf) {
  Int32 int_time;
  time_helper tm = { 0,0,0,0,0,0,0, false };
  
  /* Read the integer time from the buffer */
  dth_read32_medium(int_time, buf);
  
  /* Factor it out */
  factor_HHMMSS(& tm, int_time);
  
  /* Stringify it */
  return sprintf(str, "%s%02du:%02du:%02du", tm.is_negative ? "-" : "" ,
                 tm.hour, tm.minute, tm.second);
}

size_t dth_length_time(const NdbDictionary::Column *col, const void *buf) {
  return 16;
}

int dth_encode_time(const NdbDictionary::Column *, size_t len, 
                    const char *str, void *buf) {
  Int32  int_time;
  
  /* Make a safe (null-terminated) copy */
  DateTime_CopyBuffer copybuff(len, str);
  
  /* Turn the safe copy into an int */
  if(copybuff.too_long) return DTH_VALUE_TOO_LONG;
  if(! safe_strtol(copybuff.ptr, &int_time)) return DTH_NUMERIC_OVERFLOW;
      
  /* Store the HHMMSS int as a MEDIUM INT */
  return dth_write32_medium(int_time, (char *) buf);
}


/***** DATETIME *****/

int dth_decode_datetime(const NdbDictionary::Column *, char * &str, const void *buf) {
  Int32 int_date, int_time;
  time_helper tm = { 0,0,0,0,0,0,0, false };
  
  /* Read the datetime from the buffer.  It may not be aligned.  */
  LOAD_FOR_ARCHITECTURE(Uint64, int_datetime, buf);
  
  /* Factor it out */
  int_date = int_datetime / 1000000;
  int_time = int_datetime - (Uint64) int_date * 1000000;
  factor_HHMMSS(& tm, int_time);
  factor_YYYYMMDD(& tm, int_date);
  
  /* Stringify it */
  return sprintf(str, "%04du-%02du-%02du %02du:%02du:%02du", tm.year, tm.month, 
                 tm.day, tm.hour, tm.minute, tm.second);
 }

size_t dth_length_datetime(const NdbDictionary::Column *col, const void *buf) {
  return 20;
}

int dth_encode_datetime(const NdbDictionary::Column *, size_t len, 
                        const char *str, void *buf) {
  uint64_t int_datetime;
  
  /* Make a safe (null-terminated) copy */
  DateTime_CopyBuffer copybuff(len, str);
  
  /* Turn the safe copy into an int */
  if(copybuff.too_long) return DTH_VALUE_TOO_LONG;
  if(! safe_strtoull(copybuff.ptr, &int_datetime)) return DTH_NUMERIC_OVERFLOW;
  
  /* Store it */
  STORE_FOR_ARCHITECTURE(Uint64, int_datetime, buf);
  
  return 1;
}


/***** FLOAT and DOUBLE *****/

/* mysqld might know a desired display width for the number, but we don't.
   We use the printf("%G") defaults for float.  For double, we try to find a 
   compromise between revealing intrinsic error and losing actual precision.
   To get the length, actually print the number into a scratch buffer.
*/ 
int dth_decode_float(const NdbDictionary::Column *col, 
                     char * &str, const void *buf) {
  ALIGN_AND_LOAD(float, fval, buf);

  double dval = fval;
  return sprintf(str, "%G", dval);
}

size_t dth_length_float(const NdbDictionary::Column *col,
                        const void *buf) {
  char stack_copy[16];
  ALIGN_AND_LOAD(float, fval, buf);
  double dval = fval;
  return snprintf(stack_copy, 16, "%G", dval);
}

int dth_decode_double(const NdbDictionary::Column *col, 
                     char * &str, const void *buf) {
  ALIGN_AND_LOAD(double, dval, buf);
  return sprintf(str, "%.10F", dval);
}

size_t dth_length_double(const NdbDictionary::Column *col,
                         const void *buf) {
  char stack_copy[30];
  ALIGN_AND_LOAD(double, dval, buf);
  return snprintf(stack_copy, 30, "%.10F", dval);
}

template <typename FPTYPE> int dth_encode_fp(const NdbDictionary::Column *col, 
                                             size_t len, const char *str, 
                                             void *buf) {
  MAKE_COPY_BUFFER(64);
  errno = 0;
  double dval = strtod(copy_buff, NULL);
  if(errno == ERANGE) {
    return DTH_NUMERIC_OVERFLOW;
  }
  
  ALIGN_AND_STORE(FPTYPE, dval, buf);
  return len;
}

/***** DECIMAL *****/

int dth_decode_decimal(const NdbDictionary::Column *col, 
                       char * &str, const void *buf) {
  int scale = col->getScale();
  int prec  = col->getPrecision();
  int len = scale + prec + 3;
  decimal_bin2str(buf, col->getSizeInBytes(), prec, scale, str, len);
  return strlen(str);
}

size_t dth_length_decimal(const NdbDictionary::Column *col,
                           const void *buf) {
  return col->getScale() + col->getPrecision() + 2; // 2 for sign and point
}

int dth_encode_decimal(const NdbDictionary::Column *col, size_t len, 
                       const char *str, void *buf) {
  MAKE_COPY_BUFFER(64);
  int scale = col->getScale();
  int prec  = col->getPrecision();
  int r = decimal_str2bin(str, len, prec, scale, buf, col->getSizeInBytes());
  if(r == E_DEC_OK || r == E_DEC_TRUNCATED) {
    return len;
  }
  else {
    DEBUG_PRINT("deicmal_str2bin() returns %d", r);
    return DTH_NUMERIC_OVERFLOW;
  }
}
