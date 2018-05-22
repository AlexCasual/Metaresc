/* -*- C -*- */
/* I hate this bloody country. Smash. */
/* This file is part of Metaresc project */

#include <stdio.h>
#ifndef __USE_BSD
#define __USE_BSD
#endif /* __USE_BSD */
#include <string.h> /* strncasecmp */
#include <stdarg.h>

#include <metaresc.h>
#include <mr_stringify.h>
#include <mr_save.h>

TYPEDEF_FUNC (char *, xml_save_handler_t, (int, mr_ra_ptrdes_t *))

#define MR_XML1_DOCUMENT_HEADER "<?xml version=\"1.0\"?>"

#define MR_XML1_INDENT_SPACES (2)
#define MR_XML1_INDENT_TEMPLATE "\n%*s"
#define MR_XML1_INDENT "  "

#define MR_XML1_ATTR_CHARP " %s=\"%s\""
#define MR_XML1_ATTR_INT " %s=\"%" SCNd32 "\""

#define MR_XML1_OPEN_TAG_START "<%s"
#define MR_XML1_OPEN_TAG_END ">%s"
#define MR_XML1_OPEN_EMPTY_TAG_END "/>"
#define MR_XML1_CLOSE_TAG "</%s>"

#define XML_NONPRINT_ESC "&#x%X;"
#define ESC_SIZE (sizeof (XML_NONPRINT_ESC))
#define ESC_CHAR_MAP_SIZE (256)
static char * map[ESC_CHAR_MAP_SIZE] = {
  [0 ... ESC_CHAR_MAP_SIZE - 1] = NULL,
  [(unsigned char)'\t'] = "\t",
  [(unsigned char)'\n'] = "\n",
  [(unsigned char)'&'] = "&amp;",
  [(unsigned char)'<'] = "&lt;",
  [(unsigned char)'>'] = "&gt;",
};

/**
 * XML quote function. Escapes XML special characters.
 * @param str input string
 * @return XML quoted string
 */
static char *
xml_quote_string (char * str)
{
  int length = 0;
  char * str_;

  if (NULL == str)
    return (NULL);

  for (str_ = str; *str_; ++str_)
    if (map[(unsigned char)*str_])
      length += strlen (map[(unsigned char)*str_]);
    else if (isprint (*str_))
      ++length;
    else
      length += sizeof (XML_NONPRINT_ESC) - 1;

  str_ = MR_MALLOC (length + 1);
  if (NULL == str_)
    {
      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
      return (NULL);
    }

  length = 0;
  for (; *str; ++str)
    if (map[(unsigned char)*str])
      {
	strcpy (&str_[length], map[(unsigned char)*str]);
	length += strlen (&str_[length]);
      }
    else if (isprint (*str))
      str_[length++] = *str;
    else
      {
	sprintf (&str_[length], XML_NONPRINT_ESC, (int)(unsigned char)*str);
	length += strlen (&str_[length]);
      }
  str_[length] = 0;
  return (str_);
}

/**
 * XML unquote function. Replace XML special characters aliases on a source characters.
 * @param str input string
 * @param length length of the input string
 * @return XML unquoted string
 */
char *
xml_unquote_string (mr_substr_t * substr)
{
  char * str_ = MR_MALLOC (substr->length + 1);
  int length_ = 0;
  int i, j;

  static int inited = 0;
  static char map_c[ESC_CHAR_MAP_SIZE];
  static char * map_cp[ESC_CHAR_MAP_SIZE];
  static int map_s[ESC_CHAR_MAP_SIZE];
  static int map_size = 0;

  if (0 == inited)
    {
      for (i = 0; i < ESC_CHAR_MAP_SIZE; ++i)
	if (map[i])
	  {
	    int size = strlen (map[i]);
	    if (size > 1)
	      {
		map_c[map_size] = i;
		map_cp[map_size] = map[i];
		map_s[map_size] = size;
		++map_size;
	      }
	  }
      inited = !0;
    }

  if (NULL == str_)
    {
      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
      return (NULL);
    }

  for (j = 0; j < substr->length; ++j)
    if (substr->str[j] != '&')
      str_[length_++] = substr->str[j];
    else
      {
	char esc[ESC_SIZE];
	strncpy (esc, &substr->str[j], sizeof (esc) - 1);
	if ('#' == substr->str[j + 1])
	  {
	    int32_t code = 0;
	    int size = 0;
	    if (1 != sscanf (&substr->str[j], XML_NONPRINT_ESC "%n", &code, &size))
	      MR_MESSAGE (MR_LL_WARN, MR_MESSAGE_WRONG_XML_ESC, esc);
	    else
	      {
		j += size - 1; /* one more +1 in the loop */
		str_[length_++] = code;
	      }
	  }
	else
	  {
	    for (i = 0; i < map_size; ++i)
	      if (0 == strncasecmp (&substr->str[j], map_cp[i], map_s[i]))
		{
		  str_[length_++] = map_c[i];
		  j += map_s[i] - 1; /* one more increase in the loop */
		  break;
		}
	    if (i >= map_size)
	      {
		MR_MESSAGE (MR_LL_WARN, MR_MESSAGE_UNKNOWN_XML_ESC, esc);
		str_[length_++] = substr->str[j];
	      }
	  }
      }
  str_[length_] = 0;
  return (str_);
}

/**
 * MR_NONE type saving handler.
 * @param idx an index of node in ptrs
 * @param ptrs resizeable array with pointers descriptors
 * @return stringified representation of object
 */
static char *
xml_save_none (int idx, mr_ra_ptrdes_t * ptrs)
{
  return (NULL);
}

/**
 * MR_XXX type saving handler. Saves ptrs->ra.data[idx] as string into newly allocaeted XML node.
 * \@param idx an index of node in ptrs
 * \@param ptrs resizeable array with pointers descriptors
 * \@return stringified representation of object
 */
#define XML_SAVE_TYPE(TYPE, ...)				      \
  static char * xml_save_ ## TYPE (int idx, mr_ra_ptrdes_t * ptrs) \
  {								      \
    return (mr_stringify_ ## TYPE (&ptrs->ra[idx] __VA_ARGS__));      \
  }

XML_SAVE_TYPE (bool);
XML_SAVE_TYPE (int8_t);
XML_SAVE_TYPE (uint8_t);
XML_SAVE_TYPE (int16_t);
XML_SAVE_TYPE (uint16_t);
XML_SAVE_TYPE (int32_t);
XML_SAVE_TYPE (uint32_t);
XML_SAVE_TYPE (int64_t);
XML_SAVE_TYPE (uint64_t);
XML_SAVE_TYPE (enum);
XML_SAVE_TYPE (float);
XML_SAVE_TYPE (complex_float);
XML_SAVE_TYPE (double);
XML_SAVE_TYPE (complex_double);
XML_SAVE_TYPE (long_double_t);
XML_SAVE_TYPE (complex_long_double_t);
XML_SAVE_TYPE (bitfield);
XML_SAVE_TYPE (func);
XML_SAVE_TYPE (bitmask, , MR_BITMASK_OR_DELIMITER);

/**
 * MR_CHAR type saving handler. Stringify char.
 * @param idx an index of node in ptrs
 * @param ptrs resizeable array with pointers descriptors
 * @return stringified character value
 */
static char *
xml_save_char (int idx, mr_ra_ptrdes_t * ptrs)
{
  char str[MR_CHAR_TO_STRING_BUF_SIZE] = " ";
  str[0] = *(char*)ptrs->ra[idx].data.ptr;
  if (isprint (str[0]))
    return (xml_quote_string (str));
  sprintf (str, CINIT_CHAR_QUOTE, (int)(unsigned char)str[0]);
  return (mr_strdup (str));
}

/**
 * MR_CHAR_ARRAY type saving handler. Save char array as XML node.
 * @param idx an index of node in ptrs
 * @param ptrs resizeable array with pointers descriptors
 * @return XML escaped string content
 */
static char *
xml_save_char_array (int idx, mr_ra_ptrdes_t * ptrs)
{
  return (xml_quote_string (ptrs->ra[idx].data.ptr));
}

/**
 * MR_STRING type saving handler. Save string as XML node.
 * @param idx an index of node in ptrs
 * @param ptrs resizeable array with pointers descriptors
 * @return XML escaped string content
 */
static char *
xml1_save_string (int idx, mr_ra_ptrdes_t * ptrs)
{
  char * str = *(char**)ptrs->ra[idx].data.ptr;
  if ((NULL == str) || (ptrs->ra[idx].ref_idx >= 0))
    return (mr_strdup (""));
  else
    return (xml_quote_string (str));
}

/**
 * dummy stub for compaund types
 * @param idx an index of node in ptrs
 * @param ptrs resizeable array with pointers descriptors
 * @return empty string
 */
static char *
xml_save_empty (int idx, mr_ra_ptrdes_t * ptrs)
{
  return (mr_strdup (""));
}

/**
 * Init IO handlers Table
 */
static xml_save_handler_t xml1_save_handler[] =
  {
    [MR_TYPE_STRING] = xml1_save_string,

    [MR_TYPE_NONE] = xml_save_none,
    [MR_TYPE_VOID] = xml_save_none,
    [MR_TYPE_ENUM] = xml_save_enum,
    [MR_TYPE_BITFIELD] = xml_save_bitfield,
    [MR_TYPE_BITMASK] = xml_save_bitmask,
    [MR_TYPE_BOOL] = xml_save_bool,
    [MR_TYPE_INT8] = xml_save_int8_t,
    [MR_TYPE_UINT8] = xml_save_uint8_t,
    [MR_TYPE_INT16] = xml_save_int16_t,
    [MR_TYPE_UINT16] = xml_save_uint16_t,
    [MR_TYPE_INT32] = xml_save_int32_t,
    [MR_TYPE_UINT32] = xml_save_uint32_t,
    [MR_TYPE_INT64] = xml_save_int64_t,
    [MR_TYPE_UINT64] = xml_save_uint64_t,
    [MR_TYPE_FLOAT] = xml_save_float,
    [MR_TYPE_COMPLEX_FLOAT] = xml_save_complex_float,
    [MR_TYPE_DOUBLE] = xml_save_double,
    [MR_TYPE_COMPLEX_DOUBLE] = xml_save_complex_double,
    [MR_TYPE_LONG_DOUBLE] = xml_save_long_double_t,
    [MR_TYPE_COMPLEX_LONG_DOUBLE] = xml_save_complex_long_double_t,
    [MR_TYPE_CHAR] = xml_save_char,
    [MR_TYPE_CHAR_ARRAY] = xml_save_char_array,
    [MR_TYPE_STRUCT] = xml_save_empty,
    [MR_TYPE_FUNC] = xml_save_func,
    [MR_TYPE_FUNC_TYPE] = xml_save_func,
    [MR_TYPE_ARRAY] = xml_save_empty,
    [MR_TYPE_POINTER] = xml_save_empty,
    [MR_TYPE_UNION] = xml_save_empty,
    [MR_TYPE_ANON_UNION] = xml_save_empty,
    [MR_TYPE_NAMED_ANON_UNION] = xml_save_empty,
  };

static void *
free_and_return_null (char * content)
{
  if (content)
    MR_FREE (content);
  return (NULL);
}

/**
 * Public function. Save scheduler. Save any object as a string.
 * @param ptrs resizeable array with pointers descriptors
 * @return stringified representation of object
 */
char *
xml1_save (mr_ra_ptrdes_t * ptrs)
{
  mr_rarray_t mr_ra_str = {
    .data = { mr_strdup (MR_XML1_DOCUMENT_HEADER) },
    .MR_SIZE = sizeof (MR_XML1_DOCUMENT_HEADER),
    .type = "char",
    .alloc_size = sizeof (MR_XML1_DOCUMENT_HEADER),
  };
  int idx = 0;

  if (NULL == mr_ra_str.data.ptr)
    return (NULL);

  while (idx >= 0)
    {
      int level = 0;
      int empty_tag = !0;
      char * content = NULL;
      mr_fd_t * fdp = &ptrs->ra[idx].fd;

      /* route saving handler */
      if ((fdp->mr_type < MR_TYPE_LAST) && xml1_save_handler[fdp->mr_type])
	content = xml1_save_handler[fdp->mr_type] (idx, ptrs);
      else
	MR_MESSAGE_UNSUPPORTED_NODE_TYPE_ (fdp);

      level = MR_LIMIT_LEVEL (ptrs->ra[idx].save_params.level);
      empty_tag = (ptrs->ra[idx].first_child < 0) && ((NULL == content) || (0 == content[0]));
      if (mr_ra_printf (&mr_ra_str, MR_XML1_INDENT_TEMPLATE MR_XML1_OPEN_TAG_START, level * MR_XML1_INDENT_SPACES, "", ptrs->ra[idx].fd.name.str) < 0)
	return (free_and_return_null (content));
      if (ptrs->ra[idx].ref_idx >= 0)
	if (mr_ra_printf (&mr_ra_str, MR_XML1_ATTR_INT,
			  (ptrs->ra[idx].flags.is_content_reference) ? MR_REF_CONTENT : MR_REF,
			  ptrs->ra[ptrs->ra[idx].ref_idx].idx) < 0)
	  return (free_and_return_null (content));
      if (ptrs->ra[idx].flags.is_referenced)
	if (mr_ra_printf (&mr_ra_str, MR_XML1_ATTR_INT, MR_REF_IDX, ptrs->ra[idx].idx) < 0)
	  return (free_and_return_null (content));
      if (true == ptrs->ra[idx].flags.is_null)
	if (mr_ra_printf (&mr_ra_str, MR_XML1_ATTR_CHARP, MR_ISNULL, MR_ISNULL_VALUE) < 0)
	  return (free_and_return_null (content));

      if (empty_tag)
	{
	  if (mr_ra_printf (&mr_ra_str, MR_XML1_OPEN_EMPTY_TAG_END) < 0)
	    return (free_and_return_null (content));
	}
      else
	{
	  if (mr_ra_printf (&mr_ra_str, MR_XML1_OPEN_TAG_END, content ? content : "") < 0)
	    return (free_and_return_null (content));
	}
      if (content)
	MR_FREE (content);

      if (ptrs->ra[idx].first_child >= 0)
	idx = ptrs->ra[idx].first_child;
      else
	{
	  if (!empty_tag)
	    if (mr_ra_printf (&mr_ra_str, MR_XML1_CLOSE_TAG, ptrs->ra[idx].fd.name.str) < 0)
	      return (NULL);
	  while ((ptrs->ra[idx].next < 0) && (ptrs->ra[idx].parent >= 0))
	    {
	      idx = ptrs->ra[idx].parent;
	      level = MR_LIMIT_LEVEL (ptrs->ra[idx].save_params.level);
	      if (mr_ra_printf (&mr_ra_str, MR_XML1_INDENT_TEMPLATE MR_XML1_CLOSE_TAG, level * MR_XML1_INDENT_SPACES, "", ptrs->ra[idx].fd.name.str) < 0)
		return (NULL);
	    }
	  idx = ptrs->ra[idx].next;
	}
    }
  if (mr_ra_printf (&mr_ra_str, "\n") < 0)
    return (NULL);
  return (mr_ra_str.data.ptr);
}

#ifdef HAVE_LIBXML2
/**
 * XML string quote handler. Uses libxml encoding function.
 * @param idx an index of node in ptrs
 * @param ptrs resizeable array with pointers descriptors
 * @return XML escaped string content
 */
static char *
xml2_save_string (int idx, mr_ra_ptrdes_t * ptrs)
{
  xmlDocPtr doc = ptrs->res.data.ptr;
  xmlChar * encoded_content;
  char * content = *(char**)ptrs->ra[idx].data.ptr;

  if ((NULL == content) || (ptrs->ra[idx].ref_idx >= 0))
    content = "";
  encoded_content = xmlEncodeEntitiesReentrant (doc, BAD_CAST content);
  if (NULL == encoded_content)
    {
      content = mr_strdup ("");
      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_XML_STRING_ENCODING_FAILED, content);
    }
  else
    {
      content = mr_strdup ((char*)encoded_content);
      xmlFree (encoded_content);
    }
  return (content);
}

static xml_save_handler_t xml2_save_handler[] =
  {
    [MR_TYPE_STRING] = xml2_save_string,

    [MR_TYPE_NONE] = xml_save_none,
    [MR_TYPE_VOID] = xml_save_none,
    [MR_TYPE_ENUM] = xml_save_enum,
    [MR_TYPE_BITFIELD] = xml_save_bitfield,
    [MR_TYPE_BITMASK] = xml_save_bitmask,
    [MR_TYPE_BOOL] = xml_save_bool,
    [MR_TYPE_INT8] = xml_save_int8_t,
    [MR_TYPE_UINT8] = xml_save_uint8_t,
    [MR_TYPE_INT16] = xml_save_int16_t,
    [MR_TYPE_UINT16] = xml_save_uint16_t,
    [MR_TYPE_INT32] = xml_save_int32_t,
    [MR_TYPE_UINT32] = xml_save_uint32_t,
    [MR_TYPE_INT64] = xml_save_int64_t,
    [MR_TYPE_UINT64] = xml_save_uint64_t,
    [MR_TYPE_FLOAT] = xml_save_float,
    [MR_TYPE_COMPLEX_FLOAT] = xml_save_complex_float,
    [MR_TYPE_DOUBLE] = xml_save_double,
    [MR_TYPE_COMPLEX_DOUBLE] = xml_save_complex_double,
    [MR_TYPE_LONG_DOUBLE] = xml_save_long_double_t,
    [MR_TYPE_COMPLEX_LONG_DOUBLE] = xml_save_complex_long_double_t,
    [MR_TYPE_CHAR] = xml_save_char,
    [MR_TYPE_CHAR_ARRAY] = xml_save_char_array,
    [MR_TYPE_STRUCT] = xml_save_empty,
    [MR_TYPE_FUNC] = xml_save_func,
    [MR_TYPE_FUNC_TYPE] = xml_save_func,
    [MR_TYPE_ARRAY] = xml_save_empty,
    [MR_TYPE_POINTER] = xml_save_empty,
    [MR_TYPE_UNION] = xml_save_empty,
    [MR_TYPE_ANON_UNION] = xml_save_empty,
    [MR_TYPE_NAMED_ANON_UNION] = xml_save_empty,
  };

static mr_status_t
xml2_save_node (mr_ra_ptrdes_t * ptrs, int idx, void * context)
{
  mr_fd_t * fdp = &ptrs->ra[idx].fd;
  int parent = ptrs->ra[idx].parent;
  char number[MR_INT_TO_STRING_BUF_SIZE];
  char * content = NULL;
  xmlNodePtr node = xmlNewNode (NULL, BAD_CAST fdp->name.str);

  ptrs->ra[idx].res.data.ptr = node;
  if (NULL == node)
    {
      MR_MESSAGE (MR_LL_FATAL, MR_MESSAGE_OUT_OF_MEMORY);
      return (MR_FAILURE);
    }

  node->_private = (void*)(long)idx;

  /* route saving handler */
  if ((fdp->mr_type < MR_TYPE_LAST) && xml2_save_handler[fdp->mr_type])
    content = xml2_save_handler[fdp->mr_type] (idx, ptrs);
  else
    MR_MESSAGE_UNSUPPORTED_NODE_TYPE_ (fdp);

  if (content)
    {
      if (content[0])
	xmlNodeSetContent (node, BAD_CAST content);
      MR_FREE (content);
    }

  if (ptrs->ra[idx].ref_idx >= 0)
    {
      /* set REF_IDX property */
      sprintf (number, "%" SCNd32, ptrs->ra[ptrs->ra[idx].ref_idx].idx);
      xmlSetProp (node,
		  BAD_CAST ((ptrs->ra[idx].flags.is_content_reference) ? MR_REF_CONTENT : MR_REF),
		  BAD_CAST number);
    }
  if (ptrs->ra[idx].flags.is_referenced)
    {
      /* set IDX property */
      sprintf (number, "%" SCNd32, ptrs->ra[idx].idx);
      xmlSetProp (node, BAD_CAST MR_REF_IDX, BAD_CAST number);
    }
  if (true == ptrs->ra[idx].flags.is_null)
    xmlSetProp (node, BAD_CAST MR_ISNULL, BAD_CAST MR_ISNULL_VALUE);

  if (parent >= 0)
    xmlAddChild (ptrs->ra[parent].res.data.ptr, node);

  return (MR_SUCCESS);
}

/**
 * Public function. Save scheduler. Save any object as XML node.
 * @param ptrs resizeable array with pointers descriptors
 * @return XML document
 */
xmlDocPtr
xml2_save (mr_ra_ptrdes_t * ptrs)
{
  xmlDocPtr doc = xmlNewDoc (BAD_CAST "1.0");

  if (NULL == doc)
    {
      MR_MESSAGE (MR_LL_ERROR, MR_MESSAGE_XML_SAVE_FAILED);
      return (NULL);
    }

  ptrs->res.data.ptr = doc;
  mr_ptrs_ds (ptrs, xml2_save_node, NULL);

  if ((ptrs->size > 0) && (NULL != ptrs->ra[0].res.data.ptr))
    xmlDocSetRootElement (doc, ptrs->ra[0].res.data.ptr);

  return (doc);
}
#endif /* HAVE_LIBXML2 */
