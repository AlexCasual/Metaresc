/* -*- C -*- */
/* I hate this bloody country. Smash. */
/* This file is part of Metaresc project */

#ifdef HAVE_CONFIG_H
# include <mr_config.h>
#endif /* HAVE_CONFIG_H */

#define __USE_GNU
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

#include <metaresc.h>
#include <mr_save.h>
#include <mr_ic.h>
#include <mr_stringify.h>

#define MR_MODE DESC /* we'll need descriptors of our own types */
#include <mr_protos.h>

/* meta data for type 'char' - required as a discriminator for mr_ptr union */
MR_TYPEDEF_DESC_BI (char, "type descriptor for 'char'");
/* meta data for all scallar types */
MR_TYPEDEF_DESC_BI (string_t);
MR_TYPEDEF_DESC_BI (bool);
MR_TYPEDEF_DESC_BI (uint8_t);
MR_TYPEDEF_DESC_BI (int8_t);
MR_TYPEDEF_DESC_BI (uint16_t);
MR_TYPEDEF_DESC_BI (int16_t);
MR_TYPEDEF_DESC_BI (uint32_t);
MR_TYPEDEF_DESC_BI (int32_t);
MR_TYPEDEF_DESC_BI (uint64_t);
MR_TYPEDEF_DESC_BI (int64_t);
MR_TYPEDEF_DESC_BI (signed);
MR_TYPEDEF_DESC_BI (unsigned);
MR_TYPEDEF_DESC_BI (int);
MR_TYPEDEF_DESC_BI (short);
MR_TYPEDEF_DESC_BI (long);
MR_TYPEDEF_DESC_BI (long_int_t);
MR_TYPEDEF_DESC_BI (long_long_int_t);
MR_TYPEDEF_DESC_BI (uintptr_t);
MR_TYPEDEF_DESC_BI (intptr_t);
MR_TYPEDEF_DESC_BI (float);
MR_TYPEDEF_DESC_BI (double);
MR_TYPEDEF_DESC_BI (long_double_t);
MR_TYPEDEF_DESC_BI (complex_float_t);
MR_TYPEDEF_DESC_BI (complex_double_t);
MR_TYPEDEF_DESC_BI (complex_long_double_t);

void * mr_calloc (const char * filename, const char * function, int line, size_t count, size_t size) { return (calloc (count, size)); }
void * mr_realloc (const char * filename, const char * function, int line, void * ptr, size_t size) { return (realloc (ptr, size)); }
void mr_free (const char * filename, const char * function, int line, void * ptr) { free (ptr); }

/** Metaresc configuration structure */
mr_conf_t mr_conf = {
  .mr_mem = { /**< all memory functions may be replaced on user defined */
    .mem_alloc_strategy = 2, /**< Memory allocation strategy. Default is to double buffer every time. */
    .calloc = mr_calloc, /**< Pointer to malloc function. */
    .realloc = mr_realloc, /**< Pointer to realloc function. */
    .free = mr_free, /**< Pointer to free function. */
  },
  .log_level = MR_LL_ALL, /**< default log level ALL */
  .msg_handler = NULL, /**< pointer on user defined message handler */
  .cache_func_resolve = true,
  .type_by_name = {
    .ic_type = MR_IC_UNINITIALIZED,
  },
  .enum_by_name = {
    .ic_type = MR_IC_UNINITIALIZED,
  },
  .fields_names = {
    .ic_type = MR_IC_UNINITIALIZED,
  },
  .output_format = { [0 ... MR_TYPE_LAST - 1] = NULL, },
};

MR_MEM_INIT ( , __attribute__((constructor,weak)));

static mr_status_t mr_conf_init_visitor (mr_ptr_t key, const void * context);

static inline void
mr_conf_init ()
{
  static bool initialized = false;
  if (!initialized)
    initialized = (MR_SUCCESS == mr_ic_foreach (&mr_conf.type_by_name, mr_conf_init_visitor, NULL));
}

static mr_status_t
mr_td_visitor (mr_ptr_t key, const void * context)
{
  mr_td_t * tdp = key.ptr;
  mr_ic_free (&tdp->field_by_name);
  if (MR_TYPE_STRUCT == tdp->mr_type)
    mr_ic_free (&tdp->param.struct_param.field_by_offset);
  if (MR_TYPE_ENUM == tdp->mr_type)
    mr_ic_free (&tdp->param.enum_param.enum_by_value);
  return (MR_SUCCESS);
}

/**
 * Memory cleanp handler.
 */
static void __attribute__((destructor))
mr_cleanup (void)
{
  mr_ic_foreach (&mr_conf.type_by_name, mr_td_visitor, NULL);
  mr_ic_free (&mr_conf.enum_by_name);
  mr_ic_free (&mr_conf.type_by_name);
  mr_ic_free (&mr_conf.fields_names);
}

static int
mr_vscprintf (const char * format, va_list args) 
{
  va_list _args;
  va_copy (_args, args);
  int retval = vsnprintf (NULL, 0, format, _args);
  va_end (_args);
  return (retval);
}

static int 
mr_vasprintf (char ** strp, const char * fmt, va_list args) 
{
  *strp = NULL;
  int len = mr_vscprintf (fmt, args);
  if (len <= 0) 
    return (len);
  char * str = MR_CALLOC (len + 1, sizeof (*str));
  if (NULL == str) 
    return (-1);
  int _len = vsnprintf (str, len + 1, fmt, args);
  if (_len < 0)
    {
      MR_FREE (str);
      return (len);
    }
  *strp = str;
  return (_len);
}

/**
 * Format message. Allocates memory for message that need to be freed.
 * @param message_id message template string ID
 * @param args variadic agruments
 * @return message string allocated by standard malloc. Need to be freed outside.
 */
char *
mr_message_format (mr_message_id_t message_id, va_list args)
{
  static const char * messages[MR_MESSAGE_LAST + 1] = { [0 ... MR_MESSAGE_LAST] = NULL };
  static bool messages_inited = false;
  const char * format = "Unknown MR_MESSAGE_ID.";
  char * message = NULL;

  if (!messages_inited)
    {
      mr_td_t * tdp = &MR_DESCRIPTOR_PREFIX (mr_message_id_t);
      if (tdp)
	{
	  int i;
	  for (i = 0; MR_TYPE_ENUM_VALUE == tdp->fields[i].fdp->mr_type; ++i)
	    messages[tdp->fields[i].fdp->param.enum_value._unsigned] = tdp->fields[i].fdp->meta;
	  messages_inited = true;
	}
    }

  if ((message_id <= sizeof (messages) / sizeof (messages[0])) && messages[message_id])
    format = messages[message_id];

  int __attribute__ ((unused)) unused = mr_vasprintf (&message, format, args);

  return (message);
}

/**
 * Redirect message to user defined handler or output message to stderr.
 * @param file_name file name
 * @param func_name function name
 * @param line line number
 * @param log_level logging level of message
 * @param message_id message template string ID
 */
void
mr_message (const char * file_name, const char * func_name, int line, mr_log_level_t log_level, mr_message_id_t message_id, ...)
{
  char * message;
  va_list args;

  va_start (args, message_id);
  /* if we have user defined message handler then pass error to it */
  if (mr_conf.msg_handler)
    mr_conf.msg_handler (file_name, func_name, line, log_level, message_id, args);
  else if (log_level > mr_conf.log_level)
    {
      const char * log_level_str_ = "Unknown";
#define LL_INIT(LEVEL) [MR_LL_##LEVEL] = #LEVEL
      static const char * log_level_str[] =
	{ LL_INIT (ALL), LL_INIT (TRACE), LL_INIT (DEBUG), LL_INIT (INFO), LL_INIT (WARN), LL_INIT (ERROR), LL_INIT (FATAL), LL_INIT (OFF) };

      if (((int)log_level >= 0)
	  && ((int)log_level <= sizeof (log_level_str) / sizeof (log_level_str[0]))
	  && log_level_str[log_level])
	log_level_str_ = log_level_str[log_level];

#ifdef HAVE_EXECINFO_H
      if (log_level <= MR_LL_DEBUG)
	{
	  void * array[8];
	  size_t size;
	  char ** strings;
	  size_t i;

	  size = backtrace (array, sizeof (array) / sizeof (array[0]));
	  strings = backtrace_symbols (array, size);
	  fprintf (stderr, "Obtained %zd stack frames.\n", size);
	  if (strings)
	    {
	      for (i = 0; i < size; ++i)
		fprintf (stderr, "%s\n", strings[i]);
	      free (strings);
	    }
	}
#endif /* HAVE_EXECINFO_H */
      
      message = mr_message_format (message_id, args);
      if (NULL == message)
	fprintf (stderr, "%s: in %s %s() line %d: Out of memory formating error message #%d\n", log_level_str_, file_name, func_name, line, message_id);
      else
	{
	  fprintf (stderr, "%s: in %s %s() line %d: %s\n", log_level_str_, file_name, func_name, line, message);
	  MR_FREE (message);
	}
    }
  va_end (args);
}

/**
 * Helper function for serialization macroses. Extracts variable name that was passed for serialization.
 * Possible variants are: var_name, &var_name, &var_name[idx], &((type*)var_name)[idx], etc
 *
 * @param name string with var_name
 * @return variable name var_name
 */
char *
mr_normalize_name (char * name)
{
  char * ptr;
  ptr = strchr (name, '['); /* lookup for a bracket */
  if (NULL == ptr)          /* if bracket was not found */
    ptr = strchr (name, 0); /* use end of the string */
  --ptr;
  /* skip all invalid characters */
  while ((ptr >= name) && !(isalnum (*ptr) || ('_' == *ptr)))
    --ptr;
  *(ptr + 1) = 0; /* put end-of-string marker */
  /* all valid characters to the left forms the var_name */
  while ((ptr >= name) && (isalnum (*ptr) || ('_' == *ptr)))
    --ptr;
  return (++ptr);
}

/**
 * Allocate new string and copy first 'size' chars from str.
 * For compilers without GNU extension
 *
 * @param str string to duplicate
 * @param size size to duplicate
 * @return A pointer on newly allocated string
 */
char *
mr_strndup (const char * str, size_t size)
{
  char * _str = (char*)str;
  int _size;
  for (_size = 0; (_size < size) && *_str++; ++_size);
  _str = MR_CALLOC (_size + 1, sizeof (*_str));
  if (NULL == _str)
    {
      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
      return (NULL);
    }
  memcpy (_str, str, _size);
  _str[_size] = 0;
  return (_str);
}

char *
mr_strdup (const char * str)
{
  int _size = strlen (str) + 1;
  char * _str = MR_CALLOC (_size, sizeof (*_str));
  if (NULL == _str)
    {
      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
      return (NULL);
    }
  memcpy (_str, str, _size);
  return (_str);
}

/**
 * Extract bits of bit-field, extend sign bits if needed.
 * @param ptrdes pointer descriptor
 * @param value pointer on variable for bit-field value
 * @return status 
 */
mr_status_t
mr_save_bitfield_value (mr_ptrdes_t * ptrdes, uint64_t * value)
{
  uint8_t * ptr = ptrdes->data.ptr;
  int i;

  *value = *ptr++ >> ptrdes->fd.param.bitfield_param.shift;
  for (i = __CHAR_BIT__ - ptrdes->fd.param.bitfield_param.shift; i < ptrdes->fd.param.bitfield_param.width; i += __CHAR_BIT__)
    *value |= ((uint64_t)*ptr++) << i;
  *value &= (2LL << (ptrdes->fd.param.bitfield_param.width - 1)) - 1;
  switch (ptrdes->fd.mr_type_aux)
    {
    case MR_TYPE_INT8:
    case MR_TYPE_INT16:
    case MR_TYPE_INT32:
    case MR_TYPE_INT64:
      /* extend sign bit */
      if (*value & (1 << (ptrdes->fd.param.bitfield_param.width - 1)))
	*value |= -1 - ((2LL << (ptrdes->fd.param.bitfield_param.width - 1)) - 1);
      break;
    default:
      break;
    }
  return (MR_SUCCESS);
}

/**
 * Saves bit-field into memory
 * @param ptrdes pointer descriptor
 * @param value pointer on a memory for a bit-field store
 * @return status
 */
mr_status_t
mr_load_bitfield_value (mr_ptrdes_t * ptrdes, uint64_t * value)
{
  uint8_t * ptr = ptrdes->data.ptr;
  int i;

  *value &= (2LL << (ptrdes->fd.param.bitfield_param.width - 1)) - 1;
  if (ptrdes->fd.param.bitfield_param.shift + ptrdes->fd.param.bitfield_param.width >= __CHAR_BIT__)
    *ptr &= ((1 << ptrdes->fd.param.bitfield_param.shift) - 1);
  else
    *ptr &= (-1 - ((1 << (ptrdes->fd.param.bitfield_param.shift + ptrdes->fd.param.bitfield_param.width)) - 1)) | ((1 << ptrdes->fd.param.bitfield_param.shift) - 1);
  *ptr++ |= *value << ptrdes->fd.param.bitfield_param.shift;
  for (i = __CHAR_BIT__ - ptrdes->fd.param.bitfield_param.shift; i < ptrdes->fd.param.bitfield_param.width; i += __CHAR_BIT__)
    if (ptrdes->fd.param.bitfield_param.width - i >= __CHAR_BIT__)
      *ptr++ = *value >> i;
    else
      {
	*ptr &= -1 - ((1 << (ptrdes->fd.param.bitfield_param.width - i)) - 1);
	*ptr++ |= *value >> i;
      }
  return (MR_SUCCESS);
}

/**
 * Rarray memory allocation/reallocation
 * @param rarray a pointer on resizable array
 * @param size size of array elements
 * @return Pointer on a new element of rarray
 */
void *
mr_rarray_allocate_element (void ** data, ssize_t * size, ssize_t * alloc_size, ssize_t element_size)
{
  if ((NULL == data) || (NULL == size) || (NULL == alloc_size) ||
      (*size < 0) || (element_size < 0))
    return (NULL);
  
  char * _data = *data;
  ssize_t _size = *size;
  ssize_t new_size = _size + element_size;
  if (new_size > *alloc_size)
    {
      float mas = mr_conf.mr_mem.mem_alloc_strategy;
      ssize_t realloc_size;
      if (mas < 1)
	mas = 1;
      if (mas > 2)
	mas = 2;
      realloc_size = (((int)((new_size + 1) * mas) + element_size - 1) / element_size) * element_size;
      if (realloc_size < new_size)
	realloc_size = new_size;
      _data = MR_REALLOC (_data, realloc_size);
      if (NULL == _data)
	{
	  MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
	  return (NULL);
	}
      *alloc_size = realloc_size;
      *data = _data;
    }
  *size = new_size;
  return (&_data[_size]);
}

void *
mr_rarray_append (mr_rarray_t * rarray, ssize_t size)
{
  return (mr_rarray_allocate_element (&rarray->data.ptr, &rarray->MR_SIZE, &rarray->alloc_size, size));
}

/**
 * printf into resizable array
 * @param mr_ra_str a pointer on resizable array
 * @param format standard printf format string
 * @param ... arguments for printf
 * @return length of added content and -1 in case of memory allocation failure
 */
int
mr_ra_printf (mr_rarray_t * mr_ra_str, const char * format, ...)
{
  va_list args;
  int length, _length;

  va_start (args, format);
  length = mr_vscprintf (format, args);

  if (length < 0)
    goto free_mr_ra;

  if ((0 == mr_ra_str->MR_SIZE) || (NULL == mr_ra_str->data.ptr))
    goto free_mr_ra;

  size_t size = mr_ra_str->MR_SIZE;
  char * tail = mr_rarray_append (mr_ra_str, length);

  if (NULL == tail)
    goto free_mr_ra;
  else
    _length = vsnprintf (tail - 1, length + 1, format, args);

  if (_length < 0)
    goto free_mr_ra;
  
  mr_ra_str->MR_SIZE = _length + size;

  va_end (args);
  return (_length);

 free_mr_ra:
  if (mr_ra_str->data.ptr)
    MR_FREE (mr_ra_str->data.ptr);
  mr_ra_str->data.ptr = NULL;
  mr_ra_str->MR_SIZE = mr_ra_str->alloc_size = 0;
  va_end (args);  
  return (-1);
}

/**
 * Allocate element for pointer descriptor in resizable array.
 * @param ptrs resizable array with pointers on already saved structures
 * @return Index of pointer in the list or -1 in case of memory operation error.
 * On higher level we need index because array is always reallocating and
 * pointer on element is changing (index remains constant).
 */
int
mr_add_ptr_to_list (mr_ra_ptrdes_t * ptrs)
{
  mr_ptrdes_t * ptrdes = mr_rarray_allocate_element ((void*)&ptrs->ra, &ptrs->size, &ptrs->alloc_size, sizeof (ptrs->ra[0]));
  if (NULL == ptrdes)
    return (-1);
  memset (ptrdes, 0, sizeof (*ptrdes));
  ptrdes->idx = -1; /* NB! To be initialized in depth search in mr_save */
  ptrdes->ref_idx = -1;
  ptrdes->parent = -1;
  ptrdes->first_child = -1;
  ptrdes->last_child = -1;
  ptrdes->prev = -1;
  ptrdes->next = -1;
  return (ptrs->size / sizeof (ptrs->ra[0]) - 1);
}

/**
 * Setup referencies between parent and child node in serialization tree
 * @param parent index of parent node
 * @param child index of child node
 * @param ptrs resizable array with pointers descriptors
 */
void
mr_add_child (int parent, int child, mr_ptrdes_t * ra)
{
  int last_child;

  if (child < 0)
    return;

  ra[child].parent = parent;
  if (parent < 0)
    return;

  last_child = ra[parent].last_child;
  if (last_child < 0)
    ra[parent].first_child = child;
  else
    {
      ra[last_child].next = child;
      ra[child].prev = last_child;
      ra[child].next = -1;
    }
  ra[parent].last_child = child;
}

void
mr_assign_int (mr_ptrdes_t * dst, mr_ptrdes_t * src)
{
  uint64_t value = 0;
  mr_type_t mr_type;
  void * src_data = src->data.ptr;
  void * dst_data = dst->data.ptr;

  if (MR_TYPE_POINTER == src->fd.mr_type)
    src_data = *(void**)src_data;

  if (NULL == src_data)
    return;
  
  if (MR_TYPE_POINTER == dst->fd.mr_type)
    dst_data = *(void**)dst_data;

  if (NULL == dst_data)
    return;
  
  if ((MR_TYPE_VOID == src->fd.mr_type) || (MR_TYPE_POINTER == src->fd.mr_type))
    mr_type = src->fd.mr_type_aux;
  else
    mr_type = src->fd.mr_type;
  
  switch (mr_type)
    {
    case MR_TYPE_VOID:
    case MR_TYPE_ENUM:
      switch (src->fd.size)
	{
	case sizeof (uint8_t):
	  value = *(uint8_t*)src_data;
	  break;
	case sizeof (uint16_t):
	  value = *(uint16_t*)src_data;
	  break;
	case sizeof (uint32_t):
	  value = *(uint32_t*)src_data;
	  break;
	case sizeof (uint64_t):
	  value = *(uint64_t*)src_data;
	  break;
	}
      break;
    case MR_TYPE_BOOL:
      value = *(bool*)src_data;
      break;
    case MR_TYPE_CHAR:
      value = *(char*)src_data;
      break;
    case MR_TYPE_UINT8:
      value = *(uint8_t*)src_data;
      break;
    case MR_TYPE_INT8:
      value = *(int8_t*)src_data;
      break;
    case MR_TYPE_UINT16:
      value = *(uint16_t*)src_data;
      break;
    case MR_TYPE_INT16:
      value = *(int16_t*)src_data;
      break;
    case MR_TYPE_UINT32:
      value = *(uint32_t*)src_data;
      break;
    case MR_TYPE_INT32:
      value = *(int32_t*)src_data;
      break;
    case MR_TYPE_UINT64:
      value = *(uint64_t*)src_data;
      break;
    case MR_TYPE_INT64:
      value = *(int64_t*)src_data;
      break;
    case MR_TYPE_BITFIELD:
      mr_save_bitfield_value (src, &value); /* get value of the bitfield */
      break;
    default:
      break;
    }

  if ((MR_TYPE_VOID == dst->fd.mr_type) || (MR_TYPE_POINTER == dst->fd.mr_type))
    mr_type = dst->fd.mr_type_aux;
  else
    mr_type = dst->fd.mr_type;

  switch (mr_type)
    {
    case MR_TYPE_VOID:
    case MR_TYPE_ENUM:
      switch (dst->fd.size)
	{
	case sizeof (uint8_t):
	  *(uint8_t*)dst_data = value;
	  break;
	case sizeof (uint16_t):
	  *(uint16_t*)dst_data = value;
	  break;
	case sizeof (uint32_t):
	  *(uint32_t*)dst_data = value;
	  break;
	case sizeof (uint64_t):
	  *(uint64_t*)dst_data = value;
	  break;
	}
      break;
    case MR_TYPE_BOOL:
      *(bool*)dst_data = value;
      break;
    case MR_TYPE_CHAR:
      *(char*)dst_data = value;
      break;
    case MR_TYPE_UINT8:
      *(uint8_t*)dst_data = value;
      break;
    case MR_TYPE_INT8:
      *(int8_t*)dst_data = value;
      break;
    case MR_TYPE_UINT16:
      *(uint16_t*)dst_data = value;
      break;
    case MR_TYPE_INT16:
      *(int16_t*)dst_data = value;
      break;
    case MR_TYPE_UINT32:
      *(uint32_t*)dst_data = value;
      break;
    case MR_TYPE_INT32:
      *(int32_t*)dst_data = value;
      break;
    case MR_TYPE_UINT64:
      *(uint64_t*)dst_data = value;
      break;
    case MR_TYPE_INT64:
      *(int64_t*)dst_data = value;
      break;
    case MR_TYPE_BITFIELD:
      mr_load_bitfield_value (dst, &value); /* set value of the bitfield */
      break;
    default:
      break;
    }
}

/**
 * Checks that string is a valid field name [_a-zA-A][_a-zA-Z0-9]*
 * @param name union meta field
 */
bool
mr_is_valid_field_name (char * name)
{
  if (NULL == name)
    return (false);
  if (!isalpha (*name) && ('_' != *name))
    return (false);
  for (++name; *name; ++name)
    if (!isalnum (*name) && ('_' != *name))
      return (false);
  return (true);
}

static mr_fd_t *
mr_get_fd_by_offset (mr_td_t * tdp, mr_offset_t offset)
{
  mr_fd_t fd = { .offset = offset, };
  mr_ptr_t * result = mr_ic_find (&tdp->param.struct_param.field_by_offset, &fd);
  return (result ? result->ptr : NULL);
}

void
mr_pointer_get_size_ptrdes (mr_ptrdes_t * ptrdes, int idx, mr_ra_ptrdes_t * ptrs)
{
  char * name = NULL;
  memset (ptrdes, 0, sizeof (*ptrdes));
  
  if ((NULL != ptrs->ra[idx].fd.res_type) && (0 == strcmp ("string", ptrs->ra[idx].fd.res_type)) &&
      mr_is_valid_field_name (ptrs->ra[idx].fd.res.ptr))
    name = ptrs->ra[idx].fd.res.ptr;

  if ((NULL != ptrs->ra[idx].fd.res_type) && (0 == strcmp ("offset", ptrs->ra[idx].fd.res_type)))
    name = "";
  
  if (name != NULL)
    {
      int parent;
      /* traverse through parents up to first structure */
      for (parent = ptrs->ra[idx].parent; parent >= 0; parent = ptrs->ra[parent].parent)
	if (MR_TYPE_STRUCT == ptrs->ra[parent].fd.mr_type)
	  break;
      
      if (parent >= 0)
	{
	  mr_fd_t * parent_fdp;
	  mr_td_t * parent_tdp = mr_get_td_by_name (ptrs->ra[parent].fd.type);
  
	  if (NULL == parent_tdp)
	    return;

	  /* lookup for a size field in this parent */
	  if (0 == name[0])
	    parent_fdp = mr_get_fd_by_offset (parent_tdp, ptrs->ra[idx].fd.res.offset);
	  else
	    parent_fdp = mr_get_fd_by_name (parent_tdp, name);
	      
	  if (NULL == parent_fdp)
	    return;

	  ptrdes->fd = *parent_fdp;
	  ptrdes->data.ptr = (char*)ptrs->ra[parent].data.ptr + parent_fdp->offset; /* get an address of size field */
	}
    }
}

void
mr_pointer_set_size (int idx, mr_ra_ptrdes_t * ptrs)
{
  mr_ptrdes_t src, dst;
  mr_pointer_get_size_ptrdes (&dst, idx, ptrs);
      
  if (dst.data.ptr != NULL)
    {
      src.data.ptr = &ptrs->ra[idx].MR_SIZE;
      src.fd.mr_type = MR_TYPE_DETECT (__typeof__ (ptrs->ra[idx].MR_SIZE));
      mr_assign_int (&dst, &src);
    }
}

/**
 * Recursively free all allocated memory. Needs to be done from bottom to top.
 * @param ptrs resizable array with serialized data
 * @return status
 */
mr_status_t
mr_free_recursively (mr_ra_ptrdes_t * ptrs)
{
  int i;
  mr_status_t status = MR_SUCCESS;

  mr_conf_init ();
  
  if ((NULL == ptrs) || (NULL == ptrs->ra))
    return (MR_FAILURE);

  for (i = ptrs->size / sizeof (ptrs->ra[0]) - 1; i >= 0; --i)
    {
      mr_ptrdes_t * ptrdes = &ptrs->ra[i];
      ptrdes->res.data.ptr = NULL;

      if ((ptrdes->ref_idx < 0) && (ptrdes->idx >= 0) && !ptrdes->flags.is_null &&
	  ((MR_TYPE_POINTER == ptrdes->fd.mr_type) || (MR_TYPE_STRING == ptrdes->fd.mr_type)))
	{
	  if (NULL == ptrdes->data.ptr)
	    {
	      MR_MESSAGE (MR_LL_WARN, MR_MESSAGE_UNEXPECTED_NULL_POINTER);
	      status = MR_FAILURE;
	    }
	  else
	    ptrdes->res.data.ptr = *(void**)ptrdes->data.ptr;
	}
    }

  for (i = ptrs->size / sizeof (ptrs->ra[0]) - 1; i >= 0; --i)
    if (ptrs->ra[i].res.data.ptr)
      MR_FREE (ptrs->ra[i].res.data.ptr);

  return (status);
}

static mr_status_t
calc_relative_addr (mr_ra_ptrdes_t * ptrs, int idx, void * context)
{
  /* is new address is not set yet, then it could be calculated as relative address from the parent node */
  if (NULL == ptrs->ra[idx].res.data.ptr)
    {
      int parent = ptrs->ra[idx].parent;
      ptrs->ra[idx].res.data.ptr = &((char*)ptrs->ra[parent].res.data.ptr)[ptrs->ra[idx].data.ptr - ptrs->ra[parent].data.ptr];
    }
  return (MR_SUCCESS);
}

/**
 * Recursively copy
 * @param ptrs resizable array with serialized data
 * @return status
 */
mr_status_t
mr_copy_recursively (mr_ra_ptrdes_t * ptrs, void * dst)
{
  int i;

  mr_conf_init ();

  if ((NULL == ptrs->ra) || (NULL == dst))
    return (MR_FAILURE);

  /* copy first level struct */
  memcpy (dst, ptrs->ra[0].data.ptr, ptrs->ra[0].fd.size);
  ptrs->ra[0].res.data.ptr = dst;

  for (i = ptrs->size / sizeof (ptrs->ra[0]) - 1; i > 0; --i)
    {
      ptrs->ra[i].res.data.ptr = NULL;
      ptrs->ra[i].res.type = NULL;
    }

  /* NB index 0 is excluded */
  for (i = ptrs->size / sizeof (ptrs->ra[0]) - 1; i > 0; --i)
    /*
      process nodes that are in final save graph (ptrs->ra[i].idx >= 0)
      and are not references on other nodes (ptrs->ra[i].ref_idx < 0)
      and not a NULL pointer
    */
    if ((ptrs->ra[i].idx >= 0) && (ptrs->ra[i].ref_idx < 0) && (true != ptrs->ra[i].flags.is_null))
      switch (ptrs->ra[i].fd.mr_type)
	{
	case MR_TYPE_STRING:
	  if (*(char**)ptrs->ra[i].data.ptr != NULL)
	    {
	      ptrs->ra[i].res.type = mr_strdup (*(char**)ptrs->ra[i].data.ptr);
	      if (NULL == ptrs->ra[i].res.type)
		goto failure;
	    }
	  break;
	    
	case MR_TYPE_POINTER:
	  {
	    int idx;
	    char * copy;
	    ssize_t size = ptrs->ra[i].MR_SIZE;

	    if (ptrs->ra[i].first_child < 0)
	      {
		MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_POINTER_NODE_CHILD_MISSING,
			    ptrs->ra[i].fd.type, ptrs->ra[i].fd.name.str);
		goto failure;
	      }
	    
	    if (size < 0)
	      {
		MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_WRONG_SIZE_FOR_DYNAMIC_ARRAY, size);
		goto failure;
	      }
	    
	    copy = MR_CALLOC (1, size);
	    if (NULL == copy)
	      {
		MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
		goto failure;
	      }

	    /* copy data from source */
	    memcpy (copy, *(void**)ptrs->ra[i].data.ptr, size);
	    /* go thru all childs and calculate their addresses in newly allocated chunk */
	    for (idx = ptrs->ra[i].first_child; idx >= 0; idx = ptrs->ra[idx].next)
	      ptrs->ra[idx].res.data.ptr = &copy[(char*)ptrs->ra[idx].data.ptr - *(char**)ptrs->ra[i].data.ptr];
	  }
	  break;
	default:
	  break;
	}

  /* depth search thru the graph and calculate new addresses for all nodes */
  mr_ptrs_dfs (ptrs, calc_relative_addr, NULL);

  /* now we should update pointers in a copy */
  for (i = ptrs->size / sizeof (ptrs->ra[0]) - 1; i > 0; --i)
    if ((ptrs->ra[i].idx >= 0) && (true != ptrs->ra[i].flags.is_null)) /* skip NULL and invalid nodes */
      switch (ptrs->ra[i].fd.mr_type)
	{
	case MR_TYPE_STRING:
	  /* update pointer in the copy */
	  if (ptrs->ra[i].ref_idx < 0)
	    *(char**)ptrs->ra[i].res.data.ptr = ptrs->ra[i].res.type;
	  else if (ptrs->ra[i].flags.is_content_reference)
	    *(void**)ptrs->ra[i].res.data.ptr = ptrs->ra[ptrs->ra[i].ref_idx].res.type;
	  else
	    *(void**)ptrs->ra[i].res.data.ptr = ptrs->ra[ptrs->ra[i].ref_idx].res.data.ptr;
	  break;

	case MR_TYPE_POINTER:
	  /* update pointer in the copy */
	  if (ptrs->ra[i].ref_idx < 0)
	    *(void**)ptrs->ra[i].res.data.ptr = ptrs->ra[ptrs->ra[i].first_child].res.data.ptr;
	  else
	    *(void**)ptrs->ra[i].res.data.ptr = ptrs->ra[ptrs->ra[i].ref_idx].res.data.ptr;
	  break;

	default:
	  break;
	}

  return (MR_SUCCESS);

 failure:
  for (i = ptrs->size / sizeof (ptrs->ra[0]) - 1; i > 0; --i)
    if ((MR_TYPE_STRING == ptrs->ra[i].fd.mr_type) && (ptrs->ra[i].res.type != NULL))
      MR_FREE (ptrs->ra[i].res.type);
    else if ((MR_TYPE_POINTER == ptrs->ra[i].fd.mr_type) &&
	     (ptrs->ra[i].first_child >= 0) &&
	     (ptrs->ra[ptrs->ra[i].first_child].res.data.ptr != NULL))
      MR_FREE (ptrs->ra[ptrs->ra[i].first_child].res.data.ptr);

  return (MR_FAILURE);
}

/**
 * Default hash function.
 * @param str a pointer on null terminated string
 * @return Hash function value.
 */
mr_hash_value_t
mr_hash_str (char * str)
{
  mr_hash_value_t hash_value = 0;
  if (NULL == str)
    return (hash_value);
  while (*str)
    hash_value = (hash_value + (unsigned char)*str++) * 0xDeadBeef;
  return (hash_value);
}

mr_hash_value_t
mr_hashed_string_get_hash (const mr_hashed_string_t * x)
{
  mr_hashed_string_t * x_ = (mr_hashed_string_t*)x;
  if (0 == x_->hash_value)
    x_->hash_value = mr_hash_str (x_->str);
  return (x_->hash_value);
}

mr_hash_value_t
mr_hashed_string_get_hash_ic (mr_ptr_t x, const void * context)
{
  mr_hashed_string_t * x_ = x.ptr;
  return (mr_hashed_string_get_hash (x_));
}

/**
 * Comparator for mr_hashed_string_t
 * @param x pointer on one mr_hashed_string_t
 * @param y pointer on another mr_hashed_string_t
 * @return comparation sign
 */
int
mr_hashed_string_cmp (const mr_hashed_string_t * x, const mr_hashed_string_t * y)
{
  mr_hash_value_t x_hash_value = mr_hashed_string_get_hash (x);
  mr_hash_value_t y_hash_value = mr_hashed_string_get_hash (y);
  int diff = (x_hash_value > y_hash_value) - (x_hash_value < y_hash_value);
  if (diff)
    return (diff);
  diff = (strcmp (x->str, y->str));
  if (diff)
    return (diff);
  return (0);
}

int
mr_hashed_string_cmp_ic (const mr_ptr_t x, const mr_ptr_t y, const void * context)
{
  const mr_hashed_string_t * x_ = x.ptr;
  const mr_hashed_string_t * y_ = y.ptr;
  return (mr_hashed_string_cmp (x_, y_));
}

mr_hash_value_t
mr_fd_name_get_hash (mr_ptr_t x, const void * context)
{
  mr_fd_t * x_ = x.ptr;
  return (mr_hashed_string_get_hash (&x_->name));
}

/**
 * Comparator for mr_fd_t by name field
 * @param x pointer on one mr_fd_t
 * @param y pointer on another mr_fd_t
 * @return comparation sign
 */
int
mr_fd_name_cmp (const mr_ptr_t x, const mr_ptr_t y, const void * context)
{
  const mr_fd_t * x_ = x.ptr;
  const mr_fd_t * y_ = y.ptr;
  return (mr_hashed_string_cmp (&x_->name, &y_->name));
}

mr_hash_value_t
mr_fd_offset_get_hash (mr_ptr_t x, const void * context)
{
  mr_fd_t * x_ = x.ptr;
  return (x_->offset);
}

/**
 * Comparator for mr_fd_t by offset field
 * @param x pointer on one mr_fd_t
 * @param y pointer on another mr_fd_t
 * @return comparation sign
 */
int
mr_fd_offset_cmp (const mr_ptr_t x, const mr_ptr_t y, const void * context)
{
  const mr_fd_t * x_ = x.ptr;
  const mr_fd_t * y_ = y.ptr;
  return (x_->offset > y_->offset) - (x_->offset < y_->offset);
}

mr_hash_value_t
mr_td_name_get_hash (mr_ptr_t x, const void * context)
{
  mr_td_t * x_ = x.ptr;
  return (mr_hashed_string_get_hash (&x_->type));
}

/**
 * Comparator for mr_td_t
 * @param a pointer on one mr_td_t
 * @param b pointer on another mr_td_t
 * @return comparation sign
 */
int
mr_td_name_cmp (const mr_ptr_t x, const mr_ptr_t y, const void * context)
{
  const mr_td_t * x_ = x.ptr;
  const mr_td_t * y_ = y.ptr;
  return (mr_hashed_string_cmp (&x_->type, &y_->type));
}

/**
 * Type descriptor lookup function. Lookup by type name.
 * @param type stringified type name
 * @return pointer on type descriptor
 */
mr_td_t *
mr_get_td_by_name (char * type)
{
  mr_td_t td = { .type = { .str = type, .hash_value = mr_hash_str (type), } };
  mr_ptr_t * result = mr_ic_find (&mr_conf.type_by_name, &td);
  return (result ? result->ptr : NULL);
}

/**
 * Preprocessign of a new type. Anonymous unions should be extracted into new independant types.
 * @param tdp pointer on a new type descriptor
 * @return status
 */
static mr_status_t
mr_anon_unions_extract (mr_td_t * tdp)
{
  int count = tdp->fields_size / sizeof (tdp->fields[0]);
  int i, j;

  for (i = 0; i < count; ++i)
    {
      mr_fd_t * fdp = tdp->fields[i].fdp;
      if ((MR_TYPE_ANON_UNION == fdp->mr_type) || (MR_TYPE_NAMED_ANON_UNION == fdp->mr_type))
	{
	  static int mr_type_anonymous_union_cnt = 0;
	  mr_td_t * tdp_ = fdp->res.ptr; /* statically allocated memory for new type descriptor */
	  mr_fd_t ** first = &tdp->fields[i + 1].fdp;
	  mr_fd_t * last;
	  int opened = 1;

	  for (j = i + 1; j < count; ++j)
	    {
	      mr_fd_t * fdp_ = tdp->fields[j].fdp;
	      if ((MR_TYPE_ANON_UNION == fdp_->mr_type) ||
		  (MR_TYPE_NAMED_ANON_UNION == fdp_->mr_type))
		++opened;
	      if (MR_TYPE_END_ANON_UNION == fdp_->mr_type)
		if (0 == --opened)
		  break;
	    }
	  if (j >= count)
	    return (MR_FAILURE);

	  {
	    int fields_count = j - i; /* additional trailing element with mr_type = MR_TYPE_TRAILING_RECORD */
	    mr_fd_t * fields[fields_count];
	    /*
	      0  1  2  3  4  5  6  7  8
	      F1 AH U1 U2 AE F2 F3 F4 T
	      i = 1
	      j = 4
	      first = 2
	      fields_count = 3
	      count = 8
	    */
	    memcpy (fields, first, fields_count * sizeof (first[0]));
	    memmove (first, &first[fields_count], (count - j) * sizeof (first[0])); /* blocks overlap possible */
	    memcpy (&first[count - j], fields, fields_count * sizeof (first[0]));

	    tdp_->size = 0;
	    for (j = 0; j < fields_count - 1; ++j)
	      {
		/* offset of union memebers may differ from offset of anonymous union place holder */
		if (fields[j]->offset != 0) /* MR_VOID and MR_END_ANON_UNION has zero offset */
		  fdp->offset = fields[j]->offset;
		fields[j]->offset = 0; /* reset offset to zero */
		if (tdp_->size < fields[j]->size)
		  tdp_->size = fields[j]->size; /* find union max size member */
	      }

	    last = tdp->fields[count].fdp;
	    last->mr_type = MR_TYPE_TRAILING_RECORD; /* trailing record */
	    tdp_->mr_type = fdp->mr_type; /* MR_TYPE_ANON_UNION or MR_TYPE_NAMED_ANON_UNION */
	    sprintf (tdp_->type.str, MR_TYPE_ANONYMOUS_UNION_TEMPLATE, mr_type_anonymous_union_cnt++);
	    tdp_->type.hash_value = mr_hash_str (tdp_->type.str);
	    tdp_->attr = fdp->meta; /* anonymous union stringified attributes are saved into metas field */
	    tdp_->meta = last->meta; /* copy meta from MR_END_ANON_UNION record */
	    tdp_->fields = &tdp->fields[count - fields_count + 1];

	    fdp->meta = last->meta; /* copy meta from MR_END_ANON_UNION record */
	    tdp->fields_size -= fields_count * sizeof (tdp->fields[0]);
	    count -= fields_count;
	    fdp->type = tdp_->type.str;
	    fdp->size = tdp_->size;
	    /* set name of anonymous union to temporary type name */
	    if ((NULL == fdp->name.str) || (0 == fdp->name.str[0]))
	      fdp->name.str = fdp->type;
	    fdp->name.hash_value = mr_hash_str (fdp->name.str);

	    if (MR_SUCCESS != mr_add_type (tdp_, NULL, NULL))
	      {
		MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_ANON_UNION_TYPE_ERROR, tdp->type.str);
		return (MR_FAILURE);
	      }
	  }
	}
    }
  return (MR_SUCCESS);
}

/**
 * Gets enum value as integer
 * @param ptrdes descriptor of the saved field
 * @return enum value
 */
int64_t
mr_get_enum_value (mr_td_t * tdp, void * data)
{
  int64_t enum_value = 0;
  /*
    GCC caluculates sizeof for the type according alignment, but initialize only effective bytes
    i.e. for typedef enum __attribute__ ((packed, aligned (sizeof (uint16_t)))) {} enum_t;
    sizeof (enum_t) == 2, but type has size only 1 byte
  */
  switch (tdp->param.enum_param.mr_type_effective)
    {
    case MR_TYPE_UINT8:
      enum_value = *(uint8_t*)data;
      break;
    case MR_TYPE_INT8:
      enum_value = *(int8_t*)data;
      break;
    case MR_TYPE_UINT16:
      enum_value = *(uint16_t*)data;
      break;
    case MR_TYPE_INT16:
      enum_value = *(int16_t*)data;
      break;
    case MR_TYPE_UINT32:
      enum_value = *(uint32_t*)data;
      break;
    case MR_TYPE_INT32:
      enum_value = *(int32_t*)data;
      break;
    case MR_TYPE_UINT64:
      enum_value = *(uint64_t*)data;
      break;
    case MR_TYPE_INT64:
      enum_value = *(int64_t*)data;
      break;
    default:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      memcpy (&enum_value, data, MR_MIN (tdp->param.enum_param.size_effective, sizeof (enum_value)));
#else /*  __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ */
#error Support for non little endian architectures to be implemented
#endif /* __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ */
      break;
    }
  return (enum_value);
}

/**
 * calculate a hash value for mr_fd_t
 * @param x pointer on mr_fd_t
 * @param context pointer on a context
 * @return hash value
 */
mr_hash_value_t
mr_enumfd_get_hash (mr_ptr_t x, const void * context)
{
  mr_fd_t * fdp = x.ptr;
  return (fdp->param.enum_value._unsigned);
}

/**
 * comparator for mr_fd_t sorting by enum value
 * @param x pointer on one mr_fd_t
 * @param y pointer on another mr_fd_t
 * @return comparation sign
 */
int
cmp_enums_by_value (mr_ptr_t x, mr_ptr_t y, const void * context)
{
  mr_fd_t * x_ = x.ptr;
  mr_fd_t * y_ = y.ptr;
  return ((x_->param.enum_value._unsigned > y_->param.enum_value._unsigned) -
	  (x_->param.enum_value._unsigned < y_->param.enum_value._unsigned));
}

/**
 * New enum descriptor preprocessing. Enum literal values should be added to global lookup table and enum type descriptor should have a lookup by enum values.
 * @param tdp pointer on a new enum type descriptor
 * @return status
 */
static mr_status_t
mr_add_enum (mr_td_t * tdp)
{
  mr_ic_rarray_t mr_ic_rarray;
  int i, count = tdp->fields_size / sizeof (tdp->fields[0]);
  mr_status_t status = MR_SUCCESS;

  /*
    Enums with __attribute__((packed, aligned (XXX))) GCC generates size according alignment, but not real size which is 1 byte due to packing.
    Here we determine effective type size.
    Clang calculates size and effective size according alignment.
  */
  switch (tdp->param.enum_param.mr_type_effective)
    {
    case MR_TYPE_INT8:
    case MR_TYPE_UINT8:
      tdp->param.enum_param.size_effective = sizeof (uint8_t);
      break;
    case MR_TYPE_INT16:
    case MR_TYPE_UINT16:
      tdp->param.enum_param.size_effective = sizeof (uint16_t);
      break;
    case MR_TYPE_INT32:
    case MR_TYPE_UINT32:
      tdp->param.enum_param.size_effective = sizeof (uint32_t);
      break;
    case MR_TYPE_INT64:
    case MR_TYPE_UINT64:
      tdp->param.enum_param.size_effective = sizeof (uint64_t);
      break;
    default:
      tdp->param.enum_param.size_effective = tdp->size;
      break;
    }

  mr_ic_new (&tdp->param.enum_param.enum_by_value, mr_enumfd_get_hash, cmp_enums_by_value, "mr_fd_t", MR_IC_HASH_NEXT, NULL);
  mr_ic_rarray.ra = (mr_ptr_t*)tdp->fields;
  mr_ic_rarray.size = tdp->fields_size;
  mr_ic_rarray.alloc_size = -1;
  status = mr_ic_index (&tdp->param.enum_param.enum_by_value, &mr_ic_rarray);

  tdp->param.enum_param.is_bitmask = true;
  for (i = 0; i < count; ++i)
    {
      typeof (tdp->fields[i].fdp->param.enum_value._unsigned) value = tdp->fields[i].fdp->param.enum_value._unsigned;
      if ((value != 0) && ((value & (value - 1)) != 0))
	tdp->param.enum_param.is_bitmask = false;
      
      /* adding to global lookup table by enum literal names */
      mr_ptr_t key = { .ptr = tdp->fields[i].fdp };
      mr_ptr_t * result = mr_ic_add (&mr_conf.enum_by_name, key);
      if (NULL == result)
	{
	  status = MR_FAILURE;
	  continue;
	}
      mr_fd_t * fdp = result->ptr;
      if (fdp->param.enum_value._unsigned != value)
	{
	  MR_MESSAGE (MR_LL_WARN, MR_MESSAGE_CONFLICTED_ENUMS, fdp->name.str, tdp->type.str, value, fdp->type, fdp->param.enum_value._unsigned);
	  status = MR_FAILURE;
	}
    }

  return (status);
}

/**
 * Get enum by value. Enums type descriptors contains red-black tree with all enums forted by value.
 *
 * @param tdp pointer on a type descriptor
 * @param value enums value
 * @return pointer on enum value descriptor (mr_fd_t*) or NULL is value was not found
 */
mr_fd_t *
mr_get_enum_by_value (mr_td_t * tdp, int64_t value)
{
  mr_fd_t fd = { .param = { .enum_value = { value }, }, };
  mr_ptr_t * result = mr_ic_find (&tdp->param.enum_param.enum_by_value, &fd);
  return (result ? result->ptr : NULL);
}

/**
 * Enum literal name lookup function.
 * @param value address for enum value to store
 * @param name literal name of enum to lookup
 * @return status
 */
mr_fd_t *
mr_get_enum_by_name (char * name)
{
  mr_fd_t fd = { .name = { .str = name, .hash_value = mr_hash_str (name), } };
  mr_ptr_t * result = mr_ic_find (&mr_conf.enum_by_name, &fd);
  return (result ? result->ptr : NULL);
}

/**
 * Type name clean up. We need to drop all key words.
 * @param fdp pointer on a field descriptor
 * @return status
 */
static void
mr_normalize_type (mr_fd_t * fdp)
{
  static char * keywords[] =
    {
      "struct",
      "union",
      "enum",
      "const",
      "__const",
      "__const__",
      "volatile",
      "__volatile",
      "__volatile__",
      "restrict",
      "__restrict",
      "__restrict__",
    };
  static bool isdelimiter [1 << (__CHAR_BIT__ * sizeof (uint8_t))] =
    {
      [0 ... (1 << (__CHAR_BIT__ * sizeof (char))) - 1] = false,
      [0] = true,
      [(uint8_t)' '] = true,
      [(uint8_t)'\t'] = true,
      [(uint8_t)'*'] = true,
    };
  int i;
  char * ptr;
  int prev_is_space = 0;
  bool modified = false;

  for (i = 0; i < sizeof (keywords) / sizeof (keywords[0]); ++i)
    {
      int length = strlen (keywords[i]);
      ptr = fdp->type;
      for (;;)
	{
	  char * found = strstr (ptr, keywords[i]);
	  if (!found)
	    break;
	  if (isdelimiter[(uint8_t)found[length]] && ((found == fdp->type) || isdelimiter[(uint8_t)found[-1]]))
	    {
	      memset (found, ' ', length); /* replaced all keywords on spaces */
	      modified = true;
	    }
	  ++ptr; /* keyword might be a part of type name and we need to start search of keyword from next symbol */
	}
    }
  if (modified)
    {
      /* we need to drop all space characters */
      ptr = fdp->type;
      for (i = 0; isspace (fdp->type[i]); ++i);
      for (; fdp->type[i]; ++i)
	if (isspace (fdp->type[i]))
	  prev_is_space = !0;
	else
	  {
	    if (prev_is_space)
	      *ptr++ = ' ';
	    *ptr++ = fdp->type[i];
	    prev_is_space = 0;
	  }
      *ptr = 0;
    }
}

/**
 * Bitfield initialization. We need to calculate offset and shift. Width was initialized by macro.
 * @param fdp pointer on a field descriptor
 * @return status
 */
static void
mr_init_bitfield (mr_fd_t * fdp)
{
  int i, j;
  if ((NULL == fdp->param.bitfield_param.bitfield) ||
      (0 == fdp->param.bitfield_param.size))
    return;

  for (i = 0; i < fdp->param.bitfield_param.size; ++i)
    if (fdp->param.bitfield_param.bitfield[i])
      break;
  /* if bitmask is clear then there is no need to initialize anything */
  if (!fdp->param.bitfield_param.bitfield[i])
    return;

  fdp->offset = i;
  for (i = 0; i < __CHAR_BIT__; ++i)
    if (fdp->param.bitfield_param.bitfield[fdp->offset] & (1 << i))
      break;
  fdp->param.bitfield_param.shift = i;
  fdp->param.bitfield_param.width = 0;
  for (j = fdp->offset; j < fdp->param.bitfield_param.size; ++j)
    {
      for ( ; i < __CHAR_BIT__; ++i)
	if (fdp->param.bitfield_param.bitfield[j] & (1 << i))
	  ++fdp->param.bitfield_param.width;
	else
	  break;
      if (i < __CHAR_BIT__)
	break;
      i = 0;
    }
}

/**
 * New type descriptor preprocessing. Check fields names duplication, nornalize types name, initialize bitfields. Called once for each type.
 * @param tdp pointer on a type descriptor
 */
static void
mr_check_fields (mr_td_t * tdp)
{
  int i, count = tdp->fields_size / sizeof (tdp->fields[0]);
  for (i = 0; i < count; ++i)
    {
      mr_fd_t * fdp = tdp->fields[i].fdp;
      /*
	Check names of the fields.
	MR_VOID definitions may contain brackets (for arrays) or braces (for function pointers) or collon (for bitfields).
      */
      char * name = fdp->name.str;
      if (name)
	{
	  for (; isalnum (*name) || (*name == '_'); ++name); /* skip valid characters */
	  if (*name) /* strings with field names might be in read-only memory. For VOID names are saved in writable memory. */
	    *name = 0; /* truncate on first invalid charecter */
	}
      if (fdp->type)
	mr_normalize_type (fdp);
      if (MR_TYPE_BITFIELD == fdp->mr_type)
	mr_init_bitfield (fdp);
    }
}

void
mr_pointer_fd_set_size (mr_fd_t * fdp)
{
  static size_t types_sizes[] =
    {
      [0 ... MR_TYPE_LAST - 1] = 0,
      [MR_TYPE_VOID] = sizeof (void*),
      [MR_TYPE_BOOL] = sizeof (bool),
      [MR_TYPE_INT8] = sizeof (int8_t),
      [MR_TYPE_UINT8] = sizeof (uint8_t),
      [MR_TYPE_INT16] = sizeof (int16_t),
      [MR_TYPE_UINT16] = sizeof (uint16_t),
      [MR_TYPE_INT32] = sizeof (int32_t),
      [MR_TYPE_UINT32] = sizeof (uint32_t),
      [MR_TYPE_INT64] = sizeof (int64_t),
      [MR_TYPE_UINT64] = sizeof (uint64_t),
      [MR_TYPE_FLOAT] = sizeof (float),
      [MR_TYPE_COMPLEX_FLOAT] = sizeof (complex float),
      [MR_TYPE_DOUBLE] = sizeof (double),
      [MR_TYPE_COMPLEX_DOUBLE] = sizeof (complex double),
      [MR_TYPE_LONG_DOUBLE] = sizeof (long double),
      [MR_TYPE_COMPLEX_LONG_DOUBLE] = sizeof (complex long double),
      [MR_TYPE_CHAR] = sizeof (char),
      [MR_TYPE_STRING] = sizeof (char*),
    };

  fdp->size = types_sizes[fdp->mr_type_aux];
  if (fdp->size == 0)
    {
      mr_td_t * tdp = mr_get_td_by_name (fdp->type);
      if (tdp)
	fdp->size = tdp->size;
    }
}

/**
 * Initialize AUTO fields. Detect mr_type and pointers.
 * @param fdp pointer on a field descriptor
 */
static void
mr_auto_field_detect (mr_fd_t * fdp)
{
  mr_td_t * tdp = mr_get_td_by_name (fdp->type);
  /* check if type is in registery */
  if (tdp)
    fdp->mr_type = tdp->mr_type;
  else
    {
      /* pointers on a basic types were detected by MR_TYPE_DETECT_PTR into mr_type_aux */
      if (fdp->mr_type_aux != MR_TYPE_NONE)
	fdp->mr_type = MR_TYPE_POINTER;
      /* auto detect pointers */
      char * end = strchr (fdp->type, 0) - 1;
      if ('*' == *end)
	{
	  /* remove whitespaces before * */
	  while (isspace (end[-1]))
	    --end;
	  *end = 0; /* trancate type name */
	  fdp->mr_type = MR_TYPE_POINTER;
	}
    }
}

static void mr_fd_detect_field_type (mr_fd_t * fdp);

/**
 * Initialize fields that are pointers on functions. Detects types of arguments.
 * @param fdp pointer on a field descriptor
 * @return status
 */
static void
mr_func_field_detect (mr_fd_t * fdp)
{
  int i;
  for (i = 0; fdp->param.func_param.args[i].mr_type != MR_TYPE_TRAILING_RECORD; ++i)
    {
      mr_normalize_type (&fdp->param.func_param.args[i]);
      mr_fd_detect_field_type (&fdp->param.func_param.args[i]);
    }
  fdp->param.func_param.size = i * sizeof (fdp->param.func_param.args[0]);
}

static void
mr_fd_detect_field_type (mr_fd_t * fdp)
{
  switch (fdp->mr_type)
    {
    case MR_TYPE_NONE: /* MR_AUTO type resolution */
      mr_auto_field_detect (fdp);
      break;
      
    case MR_TYPE_FUNC:
      mr_func_field_detect (fdp);
      return;
      
    default:
      break;
    }

  mr_td_t * tdp = mr_get_td_by_name (fdp->type);
  if (NULL == tdp)
    return;
  
  switch (fdp->mr_type)
    {
      /* Enum detection */
    case MR_TYPE_INT8:
    case MR_TYPE_UINT8:
    case MR_TYPE_INT16:
    case MR_TYPE_UINT16:
    case MR_TYPE_INT32:
    case MR_TYPE_UINT32:
    case MR_TYPE_INT64:
    case MR_TYPE_UINT64:
      fdp->mr_type = tdp->mr_type;
      break;
	
      /*
	pointer, arrays and bit fields need to detect mr_type_aux for basic type
      */
    case MR_TYPE_BITFIELD:
    case MR_TYPE_POINTER:
    case MR_TYPE_ARRAY:
      fdp->mr_type_aux = tdp->mr_type;
      break;

    default:
      break;
    }
}

static mr_status_t
mr_fd_detect_res_size (mr_fd_t * fdp)
{
  if ((0 == fdp->MR_SIZE) && (fdp->res_type != NULL))
    {
      mr_td_t * res_tdp = mr_get_td_by_name (fdp->res_type);
      if (res_tdp != NULL)
	fdp->MR_SIZE = res_tdp->size;
    }
  return (MR_SUCCESS);
}

/**
 * Initialize fields descriptors. Everytnig that was not properly initialized in macro.
 * @param tdp pointer on a type descriptor
 * @param args auxiliary arguments
 * @return status
 */
void
mr_detect_fields_types (mr_td_t * tdp)
{
  int i, count = tdp->fields_size / sizeof (tdp->fields[0]);
  for (i = 0; i < count; ++i)
    {
      mr_fd_detect_field_type (tdp->fields[i].fdp);
      mr_fd_detect_res_size (tdp->fields[i].fdp);
    }
}

/**
 * Lookup field descriptor by field name
 * @param tdp a pointer on a type descriptor
 * @param name name of the field
 * @return pointer on field descriptor or NULL
 */
mr_fd_t *
mr_get_fd_by_name (mr_td_t * tdp, char * name)
{
  mr_fd_t fd = { .name = { .str = name, .hash_value = mr_hash_str (name), } };
  mr_ptr_t * result = mr_ic_find (&tdp->field_by_name, &fd);
  return (result ? result->ptr : NULL);
}

/**
 * Add type to union mr_void_ptr_t.
 * @param tdp a pointer on statically initialized type descriptor
 * @return status
 */
static mr_status_t
mr_register_type_pointer (mr_td_t * tdp)
{
  mr_fd_t * fdp;
  mr_td_t * union_tdp = mr_get_td_by_name ("mr_ptr_t");
  /* check that mr_ptr_t have already a registered */
  if (NULL == union_tdp)
    return (MR_SUCCESS);
  /* check that requested type is already registered */
  if (NULL != mr_get_fd_by_name (union_tdp, tdp->type.str))
    return (MR_SUCCESS);

  /* statically allocated trailing record is used for field descriptor */
  fdp = tdp->fields[tdp->fields_size / sizeof (tdp->fields[0])].fdp;
  if (NULL == fdp)
    {
      MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_UNEXPECTED_NULL_POINTER);
      return (MR_SUCCESS);
    }

  *fdp = *union_tdp->fields[0].fdp;
  fdp->type = tdp->type.str;
  fdp->name = tdp->type;
  fdp->size = sizeof (void *);
  fdp->offset = 0;
  fdp->mr_type = MR_TYPE_POINTER;
  fdp->mr_type_aux = tdp->mr_type;
  return ((NULL == mr_ic_add (&union_tdp->field_by_name, fdp)) ? MR_FAILURE : MR_SUCCESS);
}

static mr_status_t
fields_names_visitor (mr_ptr_t key, const void * context)
{
  mr_hashed_string_t * field_name = key.ptr;
  int field_name_length = strlen (field_name->str);
  int * max_field_name_length = (int*)context;
  if (field_name_length > *max_field_name_length)
    *max_field_name_length = field_name_length;
  return (MR_SUCCESS);
}

char *
mr_get_static_field_name (mr_substr_t * substr)
{
  static int max_field_name_length = 0;
  if (0 == max_field_name_length)
    mr_ic_foreach (&mr_conf.fields_names, fields_names_visitor, &max_field_name_length);
  
  /* protection for buffer overrun atack */
  if (substr->length > max_field_name_length)
    return (NULL);
  
  char name[substr->length + 1];
  mr_hashed_string_t hashed_string = { .str = name, .hash_value = 0, };
  memcpy (name, substr->str, substr->length);
  name[substr->length] = 0;
  mr_ptr_t * find = mr_ic_find (&mr_conf.fields_names, &hashed_string);
  mr_hashed_string_t * field_name = find ? find->ptr : NULL;
  return (field_name ? field_name->str : NULL);
}

/**
 * Add type description into repository
 * @param tdp a pointer on statically initialized type descriptor
 * @param meta meta info for the type
 * @param ... auxiliary void pointer
 * @return status
 */
mr_status_t
mr_add_type (mr_td_t * tdp, char * meta, ...)
{
  mr_status_t status = MR_SUCCESS;
  mr_ic_rarray_t mr_ic_rarray;
  va_list args;
  void * res;
  int count = 0;

  if (MR_IC_UNINITIALIZED == mr_conf.enum_by_name.ic_type)
    mr_ic_new (&mr_conf.enum_by_name, mr_fd_name_get_hash, mr_fd_name_cmp, "mr_fd_t", MR_IC_HASH_NEXT, NULL);

  if (MR_IC_UNINITIALIZED == mr_conf.type_by_name.ic_type)
    mr_ic_new (&mr_conf.type_by_name, mr_td_name_get_hash, mr_td_name_cmp, "mr_td_t", MR_IC_HASH_NEXT, NULL);
  
  if (MR_IC_UNINITIALIZED == mr_conf.fields_names.ic_type)
    mr_ic_new (&mr_conf.fields_names, mr_hashed_string_get_hash_ic, mr_hashed_string_cmp_ic, "mr_hashed_string_t", MR_IC_HASH_NEXT, NULL);

  if (NULL == tdp)
    return (MR_FAILURE); /* assert */
  /* check whether this type is already in the list */
  if (mr_get_td_by_name (tdp->type.str))
    return (MR_SUCCESS); /* this type is already registered */

  va_start (args, meta);
  res = va_arg (args, void*);
  va_end (args);

  for (count = 0; ; ++count)
    {
      mr_fd_t * fdp = tdp->fields[count].fdp;
      if (NULL == fdp)
	{
	  MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_UNEXPECTED_NULL_POINTER);
	  status = MR_FAILURE;
	}
      if (MR_TYPE_TRAILING_RECORD == fdp->mr_type)
	break;
    }
  tdp->fields_size = count * sizeof (tdp->fields[0]);

  if ((NULL != meta) && meta[0])
    tdp->meta = meta;

  if (NULL != res)
    tdp->res.ptr = res;

  if (MR_SUCCESS != mr_anon_unions_extract (tdp)) /* important to extract unions before building index over fields */
    status = MR_FAILURE;

  mr_check_fields (tdp);
  
  if (NULL == mr_ic_add (&mr_conf.fields_names, &tdp->type))
    status = MR_FAILURE;

  int i;
  for (i = 0; i < count; ++i)
    if (tdp->fields[i].fdp->name.str != NULL)
      if (NULL == mr_ic_add (&mr_conf.fields_names, &tdp->fields[i].fdp->name))
	status = MR_FAILURE;

  mr_ic_rarray.ra = (mr_ptr_t*)tdp->fields;
  mr_ic_rarray.size = tdp->fields_size;
  mr_ic_rarray.alloc_size = -1;
  
  mr_ic_new (&tdp->field_by_name, mr_fd_name_get_hash, mr_fd_name_cmp, "mr_fd_t", MR_IC_SORTED_ARRAY, NULL);
  if (MR_SUCCESS != mr_ic_index (&tdp->field_by_name, &mr_ic_rarray))
    status = MR_FAILURE;

  if (MR_TYPE_STRUCT == tdp->mr_type)
    {
      mr_ic_new (&tdp->param.struct_param.field_by_offset, mr_fd_offset_get_hash, mr_fd_offset_cmp, "mr_fd_t", MR_IC_SORTED_ARRAY, NULL);
      if (MR_SUCCESS != mr_ic_index (&tdp->param.struct_param.field_by_offset, &mr_ic_rarray))
	status = MR_FAILURE;
    }

  if (MR_TYPE_ENUM == tdp->mr_type)
    {
      if (MR_SUCCESS != mr_add_enum (tdp))
	status = MR_FAILURE;
    }

  if (NULL == mr_ic_add (&mr_conf.type_by_name, tdp))
    status = MR_FAILURE;

  return (status);
}

static void
mr_td_detect_res_size (mr_td_t * tdp)
{
  if ((0 == tdp->MR_SIZE) && (tdp->res_type != NULL))
    {
      mr_td_t * res_tdp = mr_get_td_by_name (tdp->res_type);
      if (res_tdp != NULL)
	tdp->MR_SIZE = res_tdp->size;
    }
}

static mr_status_t
mr_conf_init_visitor (mr_ptr_t key, const void * context)
{
  mr_td_t * tdp = key.ptr;
  
  mr_detect_fields_types (tdp);
  mr_td_detect_res_size (tdp);

  return (mr_register_type_pointer (tdp));
}

/**
 * Helper function for serialization macroses. Detects mr_type for enums, structures and unions.
 * Enums are detected at compile time as integers, and structures & unions as MR_TYPE_NONE
 *
 * @param fdp pointer on field descriptor
 */
void
mr_detect_type (mr_fd_t * fdp)
{
  mr_td_t * tdp;

  mr_conf_init ();

  if (NULL == fdp)
    return;
  
  mr_normalize_type (fdp);

  switch (fdp->mr_type)
    {
    case MR_TYPE_UINT8:
    case MR_TYPE_INT8:
    case MR_TYPE_UINT16:
    case MR_TYPE_INT16:
    case MR_TYPE_UINT32:
    case MR_TYPE_INT32:
    case MR_TYPE_UINT64:
    case MR_TYPE_INT64:
    case MR_TYPE_NONE:
      /* we need to detect only enums, structs and unions. string_t is declared as MR_TYPE_CHAR_ARRAY, but detected as MR_TYPE_STRING */
      tdp = mr_get_td_by_name (fdp->type);
      if (tdp)
	{
	  fdp->mr_type = tdp->mr_type;
	  fdp->size = tdp->size;
	}
      break;
    default:
      break;
    }
}

/**
 * Read into newly allocated string one xml object.
 * @param fd file descriptor
 * @return Newly allocated string with xml or NULL in case of any errors
 */
char *
mr_read_xml_doc (FILE * fd)
{
  int size, max_size;
  char * str;
  char c_ = 0;
  int opened_tags = 0;
  int tags_to_close = -1;
  int count = 2;

  max_size = 2 * 1024; /* initial string size */
  str = (char*)MR_CALLOC (max_size, sizeof (*str));
  if (NULL == str)
    {
      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
      return (NULL);
    }
  size = -1;

  for (;;)
    {
      int c = fgetc (fd);
      if ((c == EOF) || (c == 0))
	{
	  MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_UNEXPECTED_END);
	  MR_FREE (str);
	  return (NULL);
	}

      str[++size] = c;
      if (size == max_size - 1)
	{
	  void * str_;
	  max_size <<= 1; /* double input bufer size */
	  str_ = MR_REALLOC (str, max_size);
	  if (NULL == str_)
	    {
	      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
	      MR_FREE (str);
	      return (NULL);
	    }
	  str = (char*) str_;
	}

      if ((0 == opened_tags) && !(('<' == c) || isspace (c)))
	{
	  MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_UNEXPECTED_DATA);
	  MR_FREE (str);
	  return (NULL);
	}

      if ('<' == c)
	opened_tags += 2;
      if (('/' == c_) && ('>' == c))
	tags_to_close = -2;
      if (('?' == c_) && ('>' == c))
	tags_to_close = -2;
      if (('<' == c_) && ('/' == c))
	tags_to_close = -3;
      if ('>' == c)
	{
	  opened_tags += tags_to_close;
	  tags_to_close = -1;
	  if (opened_tags < 0)
	    {
	      MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_UNBALANCED_TAGS);
	      MR_FREE (str);
	      return (NULL);
	    }
	  if (0 == opened_tags)
	    if (0 == --count)
	      {
		str[++size] = 0;
		return (str);
	      }
	}
      if (!isspace (c))
	c_ = c;
    }
}
