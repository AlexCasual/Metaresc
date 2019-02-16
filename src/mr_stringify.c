/* -*- C -*- */
/* I hate this bloody country. Smash. */
/* This file is part of Metaresc project */

#include <math.h>
#include <stdbool.h>

#ifndef __USE_XOPEN2K8
#define __USE_XOPEN2K8
#endif /* __USE_XOPEN2K8 */

#include <string.h>

#ifdef HAVE_CONFIG_H
#include <mr_config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_DLFCN_H
#define __USE_GNU
#include <dlfcn.h>
#endif /* HAVE_DLFCN_H */

#include <metaresc.h>
#include <mr_stringify.h>
#include <mr_ic.h>

int mr_ra_printf_void (mr_rarray_t * mr_ra_str, mr_ptrdes_t * ptrdes)
{
  return (0);
}

int mr_ra_printf_bool_default (mr_rarray_t * mr_ra_str, mr_ptrdes_t * ptrdes)
{
  return (*(bool*)ptrdes->data.ptr ?
	  mr_ra_printf (mr_ra_str, "true") :
	  mr_ra_printf (mr_ra_str, "false"));
}

#define MR_RA_PRINTF_TMPLT(TYPE, TMPLT)					\
  int mr_ra_printf_ ## TYPE ## _default (mr_rarray_t * mr_ra_str, mr_ptrdes_t * ptrdes)	\
  { return (mr_ra_printf (mr_ra_str, TMPLT, *(TYPE *)ptrdes->data.ptr)); }
  
MR_RA_PRINTF_TMPLT (float, "%.8g")
MR_RA_PRINTF_TMPLT (double, "%.17g")
MR_RA_PRINTF_TMPLT (long_double_t, "%.20Lg")

MR_RA_PRINTF_TMPLT (int8_t, "%" SCNi8)
MR_RA_PRINTF_TMPLT (uint8_t, "%" SCNu8)
MR_RA_PRINTF_TMPLT (int16_t, "%" SCNi16)
MR_RA_PRINTF_TMPLT (uint16_t, "%" SCNu16)
MR_RA_PRINTF_TMPLT (int32_t, "%" SCNi32)
MR_RA_PRINTF_TMPLT (uint32_t, "%" SCNu32)
MR_RA_PRINTF_TMPLT (int64_t, "%" SCNi64)
MR_RA_PRINTF_TMPLT (uint64_t, "%" SCNu64)

#define MR_RA_PRINTF_COMPLEX(TYPE, SUFFIX)				\
  int mr_ra_printf_complex_ ## SUFFIX (mr_rarray_t * mr_ra_str, mr_ptrdes_t * ptrdes, char * delimiter)	\
  {									\
    int count = 0;							\
    mr_ptrdes_t _ptrdes = *ptrdes;					\
    TYPE real = __real__ *(complex TYPE *)ptrdes->data.ptr;		\
    TYPE imag = __imag__ *(complex TYPE *)ptrdes->data.ptr;		\
    _ptrdes.data.ptr = &real;						\
    count += TRY_CATCH_THROW (mr_ra_printf_ ## SUFFIX (mr_ra_str, &_ptrdes)); \
    count += TRY_CATCH_THROW (mr_ra_printf (mr_ra_str, "%s", delimiter)); \
    _ptrdes.data.ptr = &imag;						\
    count += TRY_CATCH_THROW (mr_ra_printf_ ## SUFFIX (mr_ra_str, &_ptrdes)); \
    count += TRY_CATCH_THROW (mr_ra_append_char (mr_ra_str, 'i'));	\
    return (count);							\
  }

MR_RA_PRINTF_COMPLEX (float, float)
MR_RA_PRINTF_COMPLEX (double, double)
MR_RA_PRINTF_COMPLEX (long double, long_double_t)

#define MR_RA_PRINTF_TYPE(TYPE, MR_TYPE)				\
  int mr_ra_printf_ ## TYPE (mr_rarray_t * mr_ra_str, mr_ptrdes_t * ptrdes) { \
    if (mr_conf.output_format[MR_TYPE])					\
      return (mr_conf.output_format[MR_TYPE](mr_ra_str, ptrdes));	\
    return (mr_ra_printf_ ## TYPE ## _default (mr_ra_str, ptrdes));	\
  }

MR_RA_PRINTF_TYPE (bool, MR_TYPE_BOOL);
MR_RA_PRINTF_TYPE (int8_t, MR_TYPE_INT8);
MR_RA_PRINTF_TYPE (uint8_t, MR_TYPE_UINT8);
MR_RA_PRINTF_TYPE (int16_t, MR_TYPE_INT16);
MR_RA_PRINTF_TYPE (uint16_t, MR_TYPE_UINT16);
MR_RA_PRINTF_TYPE (int32_t, MR_TYPE_INT32);
MR_RA_PRINTF_TYPE (uint32_t, MR_TYPE_UINT32);
MR_RA_PRINTF_TYPE (int64_t, MR_TYPE_INT64);
MR_RA_PRINTF_TYPE (uint64_t, MR_TYPE_UINT64);
MR_RA_PRINTF_TYPE (float, MR_TYPE_FLOAT);
MR_RA_PRINTF_TYPE (double, MR_TYPE_DOUBLE);
MR_RA_PRINTF_TYPE (long_double_t, MR_TYPE_LONG_DOUBLE);

int mr_ra_printf_enum (mr_rarray_t * mr_ra_str, mr_ptrdes_t * ptrdes)
{
  mr_td_t * tdp = mr_get_td_by_name (ptrdes->fd.type); /* look up for type descriptor */
  int size = tdp ? tdp->size_effective : ptrdes->fd.size;
  /* check whether type descriptor was found */
  if (NULL == tdp)
    MR_MESSAGE (MR_LL_WARN, MR_MESSAGE_NO_TYPE_DESCRIPTOR, ptrdes->fd.type);
  else if (MR_TYPE_ENUM != tdp->mr_type)
    MR_MESSAGE (MR_LL_WARN, MR_MESSAGE_TYPE_NOT_ENUM, ptrdes->fd.type);
  else
    {
      int64_t value = mr_get_enum_value (tdp, ptrdes->data.ptr);
      mr_fd_t * fdp = mr_get_enum_by_value (tdp, value);
      if (fdp && fdp->name.str)
	return (mr_ra_printf (mr_ra_str, "%s", fdp->name.str));
      MR_MESSAGE (MR_LL_WARN, MR_MESSAGE_SAVE_ENUM, value, tdp->type.str, ptrdes->fd.name.str);
      ptrdes->fd.size = tdp->size_effective;
    }
  /* save as integer otherwise */
  switch (size)
    {
    case sizeof (uint8_t):
      return (mr_ra_printf_uint8_t (mr_ra_str, ptrdes));
    case sizeof (uint16_t):
      return (mr_ra_printf_uint16_t (mr_ra_str, ptrdes));
    case sizeof (uint32_t):
      return (mr_ra_printf_uint32_t (mr_ra_str, ptrdes));
    case sizeof (uint64_t):
      return (mr_ra_printf_uint64_t (mr_ra_str, ptrdes));
    }
  
  return (-1);
}

int mr_ra_printf_bitfield (mr_rarray_t * mr_ra_str, mr_ptrdes_t * ptrdes)
{
  mr_ptrdes_t _ptrdes = *ptrdes;;
  uint64_t value;

  mr_save_bitfield_value (ptrdes, &value);
  _ptrdes.data.ptr = &value;

  switch (ptrdes->fd.mr_type_aux)
    {
    case MR_TYPE_BOOL: return (mr_ra_printf_bool (mr_ra_str, &_ptrdes));
    case MR_TYPE_INT8: return (mr_ra_printf_int8_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_UINT8: return (mr_ra_printf_uint8_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_INT16: return (mr_ra_printf_int16_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_UINT16: return (mr_ra_printf_uint16_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_INT32: return (mr_ra_printf_int32_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_UINT32: return (mr_ra_printf_uint32_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_INT64: return (mr_ra_printf_int64_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_UINT64: return (mr_ra_printf_uint64_t (mr_ra_str, &_ptrdes));
    case MR_TYPE_ENUM: return (mr_ra_printf_enum (mr_ra_str, &_ptrdes));
    default: break;
    }
  return (-1);
}  

TYPEDEF_STRUCT (mr_func_name_t,
		ANON_UNION (),
		(long, func_),
		(void *, func),
		END_ANON_UNION (),
		(const char *, name))

TYPEDEF_STRUCT (mr_ra_fn_t,
		(mr_func_name_t *, ra, , , { .offset = offsetof (mr_ra_fn_t, size) }, "offset"),
		(ssize_t, size),
		(ssize_t, alloc_size))

mr_hash_value_t
mr_fn_get_hash (const mr_ptr_t x, const void * context)
{
  const mr_ra_fn_t * ra_fn = context;
  return ((long)ra_fn->ra[x.long_int_t].func);
}

int
mr_fn_cmp (const mr_ptr_t x, const mr_ptr_t y, const void * context)
{
  const mr_ra_fn_t * ra_fn = context;
  return ((ra_fn->ra[x.long_int_t].func > ra_fn->ra[y.long_int_t].func) - (ra_fn->ra[x.long_int_t].func < ra_fn->ra[y.long_int_t].func));
}

static mr_ic_t cache = { .ic_type = MR_IC_UNINITIALIZED, };
static mr_ra_fn_t ra_fn = { .ra = NULL, .size = 0, .alloc_size = 0, };

static void __attribute__ ((destructor))
mr_fn_cache_cleanup (void)
{
  if (ra_fn.ra)
    MR_FREE (ra_fn.ra);
  ra_fn.ra = NULL;
  ra_fn.size = ra_fn.alloc_size = 0;
  mr_ic_free (&cache);
}

const char * mr_serialize_func (void * func)
{
#ifdef HAVE_LIBDL
  bool require_resolve = true;
  mr_func_name_t * new_fn = NULL;

  if (mr_conf.cache_func_resolve)
    {
      long_int_t idx = ra_fn.size / sizeof (ra_fn.ra[0]);

      new_fn = mr_rarray_allocate_element ((void*)&ra_fn.ra, &ra_fn.size, &ra_fn.alloc_size, sizeof (ra_fn.ra[0]));
      if (new_fn)
	{
	  new_fn->func = func;
	  new_fn->name = NULL;
      
	  if (MR_IC_UNINITIALIZED == cache.ic_type)
	    {
	      mr_res_t context = {
		.data = { &ra_fn, },
		.type = "mr_ra_fn_t",
		.MR_SIZE = sizeof (ra_fn),
	      };
	      mr_ic_new (&cache, mr_fn_get_hash, mr_fn_cmp, "long_int_t", MR_IC_HASH_NEXT, &context);
	    }
	  mr_ptr_t * add = mr_ic_add (&cache, idx);
	  if ((add != NULL) && (add->long_int_t != idx))
	    {
	      ra_fn.size -= sizeof (ra_fn.ra[0]);
	      if (ra_fn.ra[add->long_int_t].name)
		return (ra_fn.ra[add->long_int_t].name);
	      require_resolve = false;
	    }
	}
    }
  
  if (require_resolve)
    {
      Dl_info info;
      memset (&info, 0, sizeof (info));
      if (0 != dladdr (func, &info))
	{
	  if (info.dli_sname && (func == info.dli_saddr)) /* found some non-null name and address matches */
	    {
	      void * func_ = dlsym (RTLD_DEFAULT, info.dli_sname); /* try backward resolve. MAC OS X could resolve static functions, but can't make backward resolution */
	      if (func_ == func)
		{
		  if (new_fn)
		    new_fn->name = info.dli_sname;
		  return (info.dli_sname);
		}
	    }
	}
    }
#endif /* HAVE_LIBDL */
  static char buf[sizeof ("0x") + sizeof (void*) * 2];
  sprintf (buf, "%p", func);
  return (buf);
}

int mr_ra_append_char (mr_rarray_t * mr_ra_str, char c)
{
  if ((0 == mr_ra_str->MR_SIZE) || (NULL == mr_ra_str->data.ptr))
    goto free_mr_ra;

  char * tail = mr_rarray_append (mr_ra_str, sizeof (c));

  if (NULL == tail)
    goto free_mr_ra;

  tail[-1] = c;
  tail[0] = 0;
  return (sizeof (c));

 free_mr_ra:
  if (mr_ra_str->data.ptr)
    MR_FREE (mr_ra_str->data.ptr);
  mr_ra_str->data.ptr = NULL;
  mr_ra_str->MR_SIZE = mr_ra_str->alloc_size = 0;
  return (-1);
}

int mr_ra_append_string (mr_rarray_t * mr_ra_str, char * str)
{
  if ((0 == mr_ra_str->MR_SIZE) || (NULL == mr_ra_str->data.ptr))
    goto free_mr_ra;

  int length = strlen (str);
  char * tail = mr_rarray_append (mr_ra_str, length);

  if (NULL == tail)
    goto free_mr_ra;

  strcpy (&tail[-1], str);
  return (length);

 free_mr_ra:
  if (mr_ra_str->data.ptr)
    MR_FREE (mr_ra_str->data.ptr);
  mr_ra_str->data.ptr = NULL;
  mr_ra_str->MR_SIZE = mr_ra_str->alloc_size = 0;
  return (-1);
}

char mr_esc_char_map[MR_ESC_CHAR_MAP_SIZE] =
  {
    [0 ... MR_ESC_CHAR_MAP_SIZE - 1] = 0,
    [(unsigned char)'\a'] = (unsigned char)'a',
    [(unsigned char)'\b'] = (unsigned char)'b',
    [(unsigned char)'\t'] = (unsigned char)'t',
    [(unsigned char)'\n'] = (unsigned char)'n',
    [(unsigned char)'\v'] = (unsigned char)'v',
    [(unsigned char)'\f'] = (unsigned char)'f',
    [(unsigned char)'\r'] = (unsigned char)'r',
    [(unsigned char)'\\'] = (unsigned char)'\\',
  };

/**
 * Quote string.
 * @param mr_ra_str output buffer
 * @param str string pointer
 * @return number of outputed chars
 */
int mr_ra_printf_quote_string (mr_rarray_t * mr_ra_str, char * str, char * char_pattern)
{
  char * ptr;
  int count = 0;
  
  count += TRY_CATCH_THROW (mr_ra_append_char (mr_ra_str, '"'));
  
  for (ptr = str; *ptr; ++ptr)
    {
      char mapped = mr_esc_char_map[(unsigned char)*ptr];
      if (*ptr == '"')
	mapped = '"';
      if (mapped)
	{
	  count += TRY_CATCH_THROW (mr_ra_append_char (mr_ra_str, '\\'));
	  count += TRY_CATCH_THROW (mr_ra_append_char (mr_ra_str, mapped));
	}
      else if (isprint (*ptr))
	count += TRY_CATCH_THROW (mr_ra_append_char (mr_ra_str, *ptr));
      else
	count += TRY_CATCH_THROW (mr_ra_printf (mr_ra_str, char_pattern, *(unsigned char *)ptr));
    }
  
  count += TRY_CATCH_THROW (mr_ra_append_char (mr_ra_str, '"'));
  return (count);
}

