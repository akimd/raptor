/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * raptor_serialize.c - Serializers
 *
 * Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
 * 
 * This package is Free Software and part of Redland http://librdf.org/
 * 
 * It is licensed under the following three licenses as alternatives:
 *   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
 *   2. GNU General Public License (GPL) V2 or any newer version
 *   3. Apache License, V2.0 or any newer version
 * 
 * You may not use this file except in compliance with at least one of
 * the above three licenses.
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * complete terms and further detail along with the license texts for
 * the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
 * 
 */


#ifdef HAVE_CONFIG_H
#include <raptor_config.h>
#endif

#ifdef WIN32
#include <win32_raptor_config.h>
#endif


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* Raptor includes */
#include "raptor.h"
#include "raptor_internal.h"


/* prototypes for helper functions */
static raptor_serializer_factory* raptor_get_serializer_factory(raptor_world* world, const char *name);


/* helper methods */

static void
raptor_free_serializer_factory(raptor_serializer_factory* factory)
{
  RAPTOR_ASSERT_OBJECT_POINTER_RETURN(factory, raptor_serializer_factory);

  if(factory->finish_factory)
    factory->finish_factory(factory);
  
  if(factory->name)
    RAPTOR_FREE(raptor_serializer_factory, (void*)factory->name);
  if(factory->label)
    RAPTOR_FREE(raptor_serializer_factory, (void*)factory->label);
  if(factory->alias)
    RAPTOR_FREE(raptor_serializer_factory, (void*)factory->alias);
  if(factory->mime_type)
    RAPTOR_FREE(cstring, factory->mime_type);
  if(factory->uri_string)
    RAPTOR_FREE(raptor_serializer_factory, (void*)factory->uri_string);
  
  RAPTOR_FREE(raptor_serializer_factory, factory);
}


/* class methods */

int
raptor_serializers_init(raptor_world* world)
{
  int rc = 0;

  world->serializers = raptor_new_sequence((raptor_data_free_handler*)raptor_free_serializer_factory, NULL);
  if(!world->serializers)
    return 1;

  /* raptor_init_serializer_simple(); */

#ifdef RAPTOR_SERIALIZER_NTRIPLES
  rc+= raptor_init_serializer_ntriples(world) != 0;
#endif

#ifdef RAPTOR_SERIALIZER_TURTLE
  rc+= raptor_init_serializer_turtle(world) != 0;
#endif

#ifdef RAPTOR_SERIALIZER_RDFXML_ABBREV
  rc+= raptor_init_serializer_rdfxmla(world) != 0;
#endif

#ifdef RAPTOR_SERIALIZER_RDFXML
  rc+= raptor_init_serializer_rdfxml(world) != 0;
#endif

#ifdef RAPTOR_SERIALIZER_RSS_1_0
  rc+= raptor_init_serializer_rss10(world) != 0;
#endif

#ifdef RAPTOR_SERIALIZER_ATOM
  rc+= raptor_init_serializer_atom(world) != 0;
#endif

#ifdef RAPTOR_SERIALIZER_DOT
  rc+= raptor_init_serializer_dot(world) != 0;
#endif

#ifdef RAPTOR_SERIALIZER_JSON
  rc+= raptor_init_serializer_json(world) != 0;
#endif

  return rc;
}


/*
 * raptor_serializers_finish - delete all the registered serializers
 */
void
raptor_serializers_finish(raptor_world* world)
{
  if(world->serializers) {
    raptor_free_sequence(world->serializers);
    world->serializers = NULL;
  }
}


/*
 * raptor_serializer_register_factory - Register a syntax that can be generated by a serializer factory
 * @world: raptor_world object
 * @name: the short syntax name
 * @label: readable label for syntax
 * @mime_type: MIME type of the syntax generated by the serializer (or NULL)
 * @uri_string: URI string of the syntax (or NULL)
 * @factory: pointer to function to call to register the factory
 * 
 * INTERNAL
 *
 * Return value: non-0 on failure
 **/
RAPTOR_EXTERN_C
int
raptor_serializer_register_factory(raptor_world* world,
                                   const char *name, const char *label,
                                   const char *mime_type,
                                   const char *alias,
                                   const unsigned char *uri_string,
                                   int (*factory) (raptor_serializer_factory*)) 
{
  raptor_serializer_factory *serializer;
  char *name_copy, *label_copy, *mime_type_copy, *alias_copy;
  unsigned char *uri_string_copy;
  int i;
  
#if defined(RAPTOR_DEBUG) && RAPTOR_DEBUG > 1
  RAPTOR_DEBUG4("Received registration for syntax serializer %s '%s' with alias '%s'\n", 
                name, label, (alias ? alias : "none"));
  RAPTOR_DEBUG3("MIME type %s, URI %s\n", 
                (mime_type ? mime_type : "none"),
                (uri_string ? (const char *)uri_string : "none"));
#endif
  
  for(i = 0;
      (serializer = (raptor_serializer_factory*)raptor_sequence_get_at(world->serializers, i));
      i++) {
    if(!strcmp(serializer->name, name)) {
      RAPTOR_FATAL2("serializer %s already registered\n", name);
      return 1;
    }
  }
  

  serializer = (raptor_serializer_factory*)RAPTOR_CALLOC(raptor_serializer_factory, 1,
                                               sizeof(*serializer));
  if(!serializer)
    return 1;

  serializer->world = world;

  name_copy = (char*)RAPTOR_CALLOC(cstring, strlen(name)+1, 1);
  if(!name_copy)
    goto tidy;
  strcpy(name_copy, name);
  serializer->name = name_copy;
        
  label_copy = (char*)RAPTOR_CALLOC(cstring, strlen(label)+1, 1);
  if(!label_copy)
    goto tidy;
  strcpy(label_copy, label);
  serializer->label = label_copy;

  if(mime_type) {
    mime_type_copy = (char*)RAPTOR_CALLOC(cstring, strlen(mime_type)+1, 1);
    if(!mime_type_copy)
      goto tidy;
    strcpy(mime_type_copy, mime_type);
    serializer->mime_type = mime_type_copy;
  }

  if(uri_string) {
    uri_string_copy = (unsigned char*)RAPTOR_CALLOC(cstring, strlen((const char*)uri_string)+1, 1);
    if(!uri_string_copy)
      goto tidy;
    strcpy((char*)uri_string_copy, (const char*)uri_string);
    serializer->uri_string = uri_string_copy;
  }
        
  if(alias) {
    alias_copy = (char*)RAPTOR_CALLOC(cstring, strlen(alias)+1, 1);
    if(!alias_copy)
      goto tidy;
    strcpy(alias_copy, alias);
    serializer->alias = alias_copy;
  }

  if(raptor_sequence_push(world->serializers, serializer))
    return 1; /* on error, serializer is already freed by the sequence */

  /* Call the serializer registration function on the new object */
  if(factory(serializer))
    return 1; /* serializer is owned and freed by the serializers sequence */
  
#if defined(RAPTOR_DEBUG) && RAPTOR_DEBUG > 1
  RAPTOR_DEBUG3("%s has context size %d\n", name, serializer->context_length);
#endif

  return 0;

  /* Clean up on failure */
  tidy:
  raptor_free_serializer_factory(serializer);
  return 1;
}


/**
 * raptor_get_serializer_factory:
 * @world: raptor_world object
 * @name: the factory name or NULL for the default factory
 *
 * Get a serializer factory by name.
 * 
 * Return value: the factory object or NULL if there is no such factory
 **/
static raptor_serializer_factory*
raptor_get_serializer_factory(raptor_world* world, const char *name) 
{
  raptor_serializer_factory *factory;

  /* return 1st serializer if no particular one wanted - why? */
  if(!name) {
    factory = (raptor_serializer_factory *)raptor_sequence_get_at(world->serializers, 0);
    if(!factory) {
      RAPTOR_DEBUG1("No (default) serializers registered\n");
      return NULL;
    }
  } else {
    int i;
    
    for(i = 0;
        (factory = (raptor_serializer_factory*)raptor_sequence_get_at(world->serializers, i));
        i++) {
      if(!strcmp(factory->name, name) ||
         (factory->alias && !strcmp(factory->alias, name)))
        break;

    }

    /* else FACTORY name not found */
    if(!factory) {
      RAPTOR_DEBUG2("No serializer with name %s found\n", name);
      return NULL;
    }
  }
        
  return factory;
}


/**
 * raptor_world_enumerate_serializers:
 * @world: raptor_world object
 * @counter: index into the list of syntaxes
 * @name: pointer to store the name of the syntax (or NULL)
 * @label: pointer to store syntax readable label (or NULL)
 * @mime_type: pointer to store syntax MIME Type (or NULL)
 * @uri_string: pointer to store syntax URI string (or NULL)
 *
 * Get information on syntax serializers.
 * 
 * Return value: non 0 on failure of if counter is out of range
 **/
int
raptor_world_enumerate_serializers(raptor_world* world,
                                   const unsigned int counter,
                                   const char **name, const char **label,
                                   const char **mime_type,
                                   const unsigned char **uri_string)
{
  raptor_serializer_factory *factory;

  factory = (raptor_serializer_factory*)raptor_sequence_get_at(world->serializers,
                                                             counter);

  if(!factory)
    return 1;

  if(name)
    *name=factory->name;
  if(label)
    *label=factory->label;
  if(mime_type)
    *mime_type=factory->mime_type;
  if(uri_string)
    *uri_string=factory->uri_string;

  return 0;
}


/**
 * raptor_world_is_serializer_name:
 * @world: raptor_world object
 * @name: the syntax name
 *
 * Check name of a serializer.
 *
 * Return value: non 0 if name is a known syntax name
 */
int
raptor_world_is_serializer_name(raptor_world* world, const char *name)
{
  return (raptor_get_serializer_factory(world, name) != NULL);
}


/**
 * raptor_new_serializer:
 * @world: raptor_world object
 * @name: the serializer name
 *
 * Constructor - create a new raptor_serializer object.
 *
 * Return value: a new #raptor_serializer object or NULL on failure
 */
raptor_serializer*
raptor_new_serializer(raptor_world* world, const char *name)
{
  raptor_serializer_factory* factory;
  raptor_serializer* rdf_serializer;

  factory = raptor_get_serializer_factory(world, name);
  if(!factory)
    return NULL;

  rdf_serializer = (raptor_serializer*)RAPTOR_CALLOC(raptor_serializer, 1,
                                                     sizeof(*rdf_serializer));
  if(!rdf_serializer)
    return NULL;

  rdf_serializer->world = world;
  
  rdf_serializer->context = (char*)RAPTOR_CALLOC(raptor_serializer_context, 1,
                                               factory->context_length);
  if(!rdf_serializer->context) {
    raptor_free_serializer(rdf_serializer);
    return NULL;
  }
  
  rdf_serializer->factory = factory;

  /* Default options */
  
  /* Emit @base directive or equivalent */
  rdf_serializer->option_write_base_uri = 1;
  
  /* Emit relative URIs where possible */
  rdf_serializer->option_relative_uris = 1;

  rdf_serializer->option_resource_border  =
    rdf_serializer->option_literal_border =
    rdf_serializer->option_bnode_border   =
    rdf_serializer->option_resource_fill  =
    rdf_serializer->option_literal_fill   =
    rdf_serializer->option_bnode_fill     = NULL;

  /* XML 1.0 output */
  rdf_serializer->xml_version = 10;

  /* Write XML declaration */
  rdf_serializer->option_write_xml_declaration = 1;

  /* JSON callback function name */
  rdf_serializer->option_json_callback= NULL;

  /* JSON extra data */
  rdf_serializer->option_json_extra_data= NULL;

  /* RSS triples */
  rdf_serializer->option_rss_triples= NULL;

  /* Atom entry URI */
  rdf_serializer->option_atom_entry_uri= NULL;

  /* Prefix elements with a namespace */
  rdf_serializer->option_prefix_elements = 0;

  if(factory->init(rdf_serializer, name)) {
    raptor_free_serializer(rdf_serializer);
    return NULL;
  }
  
  return rdf_serializer;
}


/**
 * raptor_serialize_start_to_iostream:
 * @rdf_serializer:  the #raptor_serializer
 * @uri: base URI or NULL if no base URI is required
 * @iostream: #raptor_iostream to write serialization to
 * 
 * Start serialization to an iostream with given base URI
 *
 * The passed in @iostream does not becomes owned by the serializer
 * and can be used by the caller after serializing is done.  It
 * must be destroyed by the caller.
 *
 * Return value: non-0 on failure.
 **/
int
raptor_serialize_start_to_iostream(raptor_serializer *rdf_serializer,
                                   raptor_uri *uri, raptor_iostream *iostream) 
{
  if(rdf_serializer->base_uri)
    raptor_free_uri(rdf_serializer->base_uri);

  if(!iostream)
    return 1;
  
  if(uri)
    uri = raptor_uri_copy(uri);
  
  rdf_serializer->base_uri = uri;
  rdf_serializer->locator.uri = uri;
  rdf_serializer->locator.line = rdf_serializer->locator.column = 0;

  rdf_serializer->iostream = iostream;

  if(rdf_serializer->factory->serialize_start)
    return rdf_serializer->factory->serialize_start(rdf_serializer);
  return 0;
}


/**
 * raptor_serialize_start_to_filename:
 * @rdf_serializer:  the #raptor_serializer
 * @filename:  filename to serialize to
 *
 * Start serializing to a filename.
 * 
 * Return value: non-0 on failure.
 **/
int
raptor_serialize_start_to_filename(raptor_serializer *rdf_serializer,
                                   const char *filename)
{
  unsigned char *uri_string = raptor_uri_filename_to_uri_string(filename);
  if(!uri_string)
    return 1;

  if(rdf_serializer->base_uri)
    raptor_free_uri(rdf_serializer->base_uri);

  rdf_serializer->base_uri = raptor_new_uri(rdf_serializer->world, uri_string);
  rdf_serializer->locator.uri = rdf_serializer->base_uri;
  rdf_serializer->locator.line = rdf_serializer->locator.column = 0;

  RAPTOR_FREE(cstring, uri_string);

  rdf_serializer->iostream = raptor_new_iostream_to_filename(rdf_serializer->world,
                                                             filename);
  if(!rdf_serializer->iostream)
    return 1;

  rdf_serializer->free_iostream_on_end = 1;

  if(rdf_serializer->factory->serialize_start)
    return rdf_serializer->factory->serialize_start(rdf_serializer);
  return 0;
}



/**
 * raptor_serialize_start_to_string:
 * @rdf_serializer:  the #raptor_serializer
 * @uri: base URI or NULL if no base URI is required
 * @string_p: pointer to location to hold string
 * @length_p: pointer to location to hold length of string (or NULL)
 *
 * Start serializing to a string.
 * 
 * Return value: non-0 on failure.
 **/
int
raptor_serialize_start_to_string(raptor_serializer *rdf_serializer,
                                 raptor_uri *uri,
                                 void **string_p, size_t *length_p) 
{
  if(rdf_serializer->base_uri)
    raptor_free_uri(rdf_serializer->base_uri);

  if(uri)
    rdf_serializer->base_uri = raptor_uri_copy(uri);
  else
    rdf_serializer->base_uri = NULL;
  rdf_serializer->locator.uri = rdf_serializer->base_uri;
  rdf_serializer->locator.line = rdf_serializer->locator.column = 0;


  rdf_serializer->iostream = raptor_new_iostream_to_string(rdf_serializer->world,
                                                           string_p, length_p, 
                                                           NULL);
  if(!rdf_serializer->iostream)
    return 1;

  rdf_serializer->free_iostream_on_end = 1;

  if(rdf_serializer->factory->serialize_start)
    return rdf_serializer->factory->serialize_start(rdf_serializer);
  return 0;
}


/**
 * raptor_serialize_start_to_file_handle:
 * @rdf_serializer:  the #raptor_serializer
 * @uri: base URI or NULL if no base URI is required
 * @fh:  FILE* to serialize to
 *
 * Start serializing to a FILE*.
 * 
 * NOTE: This does not fclose the handle when it is finished.
 *
 * Return value: non-0 on failure.
 **/
int
raptor_serialize_start_to_file_handle(raptor_serializer *rdf_serializer,
                                      raptor_uri *uri, FILE *fh) 
{
  if(rdf_serializer->base_uri)
    raptor_free_uri(rdf_serializer->base_uri);

  if(uri)
    rdf_serializer->base_uri = raptor_uri_copy(uri);
  else
    rdf_serializer->base_uri = NULL;
  rdf_serializer->locator.uri = rdf_serializer->base_uri;
  rdf_serializer->locator.line = rdf_serializer->locator.column = 0;

  rdf_serializer->iostream = raptor_new_iostream_to_file_handle(rdf_serializer->world, fh);
  if(!rdf_serializer->iostream)
    return 1;

  rdf_serializer->free_iostream_on_end = 1;

  if(rdf_serializer->factory->serialize_start)
    return rdf_serializer->factory->serialize_start(rdf_serializer);
  return 0;
}


/**
 * raptor_serialize_set_namespace:
 * @rdf_serializer: the #raptor_serializer
 * @uri: #raptor_uri of namespace or NULL
 * @prefix: prefix to use or NULL
 *
 * set a namespace uri/prefix mapping for serializing.
 *
 * return value: non-0 on failure.
 **/
int
raptor_serialize_set_namespace(raptor_serializer* rdf_serializer,
                               raptor_uri *uri, const unsigned char *prefix) 
{
  if(prefix && !*prefix)
    prefix = NULL;
  
  if(rdf_serializer->factory->declare_namespace)
    return rdf_serializer->factory->declare_namespace(rdf_serializer, 
                                                      uri, prefix);

  return 1;
}


/**
 * raptor_serialize_set_namespace_from_namespace:
 * @rdf_serializer: the #raptor_serializer
 * @nspace: #raptor_namespace to set
 *
 * Set a namespace uri/prefix mapping for serializing from an existing namespace.
 *
 * Return value: non-0 on failure.
 **/
int
raptor_serialize_set_namespace_from_namespace(raptor_serializer* rdf_serializer,
                                              raptor_namespace *nspace)
{
  if(rdf_serializer->factory->declare_namespace_from_namespace)
    return rdf_serializer->factory->declare_namespace_from_namespace(rdf_serializer, 
                                                                     nspace);
  else if(rdf_serializer->factory->declare_namespace)
    return rdf_serializer->factory->declare_namespace(rdf_serializer, 
                                                      raptor_namespace_get_uri(nspace),
                                                      raptor_namespace_get_prefix(nspace));

  return 1;
}


/**
 * raptor_serialize_statement:
 * @rdf_serializer: the #raptor_serializer
 * @statement: #raptor_statement to serialize to a syntax
 *
 * Serialize a statement.
 * 
 * Return value: non-0 on failure.
 **/
int
raptor_serialize_statement(raptor_serializer* rdf_serializer,
                           raptor_statement *statement)
{
  if(!rdf_serializer->iostream)
    return 1;
  return rdf_serializer->factory->serialize_statement(rdf_serializer, statement);
}


/**
 * raptor_serialize_end:
 * @rdf_serializer:  the #raptor_serializer
 *
 * End a serialization.
 * 
 * Return value: non-0 on failure.
 **/
int
raptor_serialize_end(raptor_serializer *rdf_serializer) 
{
  int rc;
  
  if(!rdf_serializer->iostream)
    return 1;

  if(rdf_serializer->factory->serialize_end)
    rc = rdf_serializer->factory->serialize_end(rdf_serializer);
  else
    rc = 0;

  if(rdf_serializer->iostream) {
    if(rdf_serializer->free_iostream_on_end)
      raptor_free_iostream(rdf_serializer->iostream);
    rdf_serializer->iostream = NULL;
  }
  return rc;
}



/**
 * raptor_free_serializer:
 * @rdf_serializer: #raptor_serializer object
 *
 * Destructor - destroy a raptor_serializer object.
 * 
 **/
void
raptor_free_serializer(raptor_serializer* rdf_serializer) 
{
  RAPTOR_ASSERT_OBJECT_POINTER_RETURN(rdf_serializer, raptor_serializer);

  if(rdf_serializer->factory)
    rdf_serializer->factory->terminate(rdf_serializer);

  if(rdf_serializer->context)
    RAPTOR_FREE(raptor_serializer_context, rdf_serializer->context);

  if(rdf_serializer->base_uri)
    raptor_free_uri(rdf_serializer->base_uri);

  if(rdf_serializer->option_resource_border)
    RAPTOR_FREE(cstring, rdf_serializer->option_resource_border);
  
  if(rdf_serializer->option_literal_border)
    RAPTOR_FREE(cstring, rdf_serializer->option_literal_border);
  
  if(rdf_serializer->option_bnode_border)
    RAPTOR_FREE(cstring, rdf_serializer->option_bnode_border);
  
  if(rdf_serializer->option_resource_fill)
    RAPTOR_FREE(cstring, rdf_serializer->option_resource_fill);
  
  if(rdf_serializer->option_literal_fill)
    RAPTOR_FREE(cstring, rdf_serializer->option_literal_fill);
  
  if(rdf_serializer->option_bnode_fill)
    RAPTOR_FREE(cstring, rdf_serializer->option_bnode_fill);
  
  if(rdf_serializer->option_json_callback)
    RAPTOR_FREE(cstring, rdf_serializer->option_json_callback);

  if(rdf_serializer->option_json_extra_data)
    RAPTOR_FREE(cstring, rdf_serializer->option_json_extra_data);

  if(rdf_serializer->option_rss_triples)
    RAPTOR_FREE(cstring, rdf_serializer->option_rss_triples);

  if(rdf_serializer->option_atom_entry_uri)
    RAPTOR_FREE(cstring, rdf_serializer->option_atom_entry_uri);

  RAPTOR_FREE(raptor_serializer, rdf_serializer);
}


/**
 * raptor_serializer_get_iostream:
 * @serializer: #raptor_serializer object
 *
 * Get the current serializer iostream.
 *
 * Return value: the serializer's current iostream or NULL if 
 **/
raptor_iostream*
raptor_serializer_get_iostream(raptor_serializer *serializer)
{
  return serializer->iostream;
}


/**
 * raptor_serializer_set_option:
 * @serializer: #raptor_serializer serializer object
 * @option: option to set from enumerated #raptor_option values
 * @value: integer option value (0 or larger)
 *
 * Set serializer options with integer values.
 * 
 * The allowed options are available via 
 * raptor_world_enumerate_serializer_options().
 *
 * Return value: non 0 on failure or if the option is unknown
 **/
int
raptor_serializer_set_option(raptor_serializer *serializer, 
                              raptor_option option, int value)
{
  if(value < 0 ||
     !raptor_option_is_valid_for_area(option, RAPTOR_OPTION_AREA_SERIALIZER))
    return -1;
  
  switch(option) {
    case RAPTOR_OPTION_WRITE_BASE_URI:
      serializer->option_write_base_uri = value;
      break;

    case RAPTOR_OPTION_RELATIVE_URIS:
      serializer->option_relative_uris = value;
      break;

    case RAPTOR_OPTION_WRITER_XML_VERSION:
      if(value == 10 || value == 11)
        serializer->xml_version = value;
      break;

    case RAPTOR_OPTION_WRITER_XML_DECLARATION:
      serializer->option_write_xml_declaration = value;
      break;

    case RAPTOR_OPTION_PREFIX_ELEMENTS:
      serializer->option_prefix_elements = value;
      break;

    /* parser options */
    case RAPTOR_OPTION_SCANNING:
    case RAPTOR_OPTION_ALLOW_NON_NS_ATTRIBUTES:
    case RAPTOR_OPTION_ALLOW_OTHER_PARSETYPES:
    case RAPTOR_OPTION_ALLOW_BAGID:
    case RAPTOR_OPTION_ALLOW_RDF_TYPE_RDF_LIST:
    case RAPTOR_OPTION_NORMALIZE_LANGUAGE:
    case RAPTOR_OPTION_NON_NFC_FATAL:
    case RAPTOR_OPTION_WARN_OTHER_PARSETYPES:
    case RAPTOR_OPTION_CHECK_RDF_ID:
    case RAPTOR_OPTION_HTML_TAG_SOUP:
    case RAPTOR_OPTION_MICROFORMATS:
    case RAPTOR_OPTION_HTML_LINK:
    case RAPTOR_OPTION_WWW_TIMEOUT:

    /* Shared */
    case RAPTOR_OPTION_NO_NET:

    /* XML writer options */
    case RAPTOR_OPTION_WRITER_AUTO_INDENT:
    case RAPTOR_OPTION_WRITER_AUTO_EMPTY:
    case RAPTOR_OPTION_WRITER_INDENT_WIDTH:

    /* String options */
    case RAPTOR_OPTION_RESOURCE_BORDER:
    case RAPTOR_OPTION_LITERAL_BORDER:
    case RAPTOR_OPTION_BNODE_BORDER:
    case RAPTOR_OPTION_RESOURCE_FILL:
    case RAPTOR_OPTION_LITERAL_FILL:
    case RAPTOR_OPTION_BNODE_FILL:
    case RAPTOR_OPTION_JSON_CALLBACK:
    case RAPTOR_OPTION_JSON_EXTRA_DATA:
    case RAPTOR_OPTION_RSS_TRIPLES:
    case RAPTOR_OPTION_ATOM_ENTRY_URI:

    /* WWW options */
    case RAPTOR_OPTION_WWW_HTTP_CACHE_CONTROL:
    case RAPTOR_OPTION_WWW_HTTP_USER_AGENT:

    default:
      return -1;
      break;
  }

  return 0;
}


static int
raptor_serializer_copy_string(unsigned char ** dest,
			      const unsigned char * src)
{
  size_t src_len = strlen((const char *)src);

  if(*dest) {
    RAPTOR_FREE(cstring, *dest);
    *dest=NULL;
  }

  if(!(*dest = (unsigned char*)RAPTOR_MALLOC(cstring, src_len+1)))
    return -1;

  strcpy((char *)(*dest), (const char *)src);

  return 0;
}


/**
 * raptor_serializer_set_option_string:
 * @serializer: #raptor_serializer serializer object
 * @option: option to set from enumerated #raptor_option values
 * @value: option value
 *
 * Set serializer options with string values.
 * 
 * The allowed options are available via
 * raptor_world_enumerate_serializer_options().
 * If the option type is integer, the value is interpreted as an integer.
 *
 * Return value: non 0 on failure or if the option is unknown
 **/
int
raptor_serializer_set_option_string(raptor_serializer *serializer, 
                                     raptor_option option, 
                                     const unsigned char *value)
{
  if(!value ||
     !raptor_option_is_valid_for_area(option, RAPTOR_OPTION_AREA_SERIALIZER))
    return -1;
  
  if(raptor_option_value_is_numeric(option))
    return raptor_serializer_set_option(serializer, option, 
                                         atoi((const char*)value));

  switch(option) {
    case RAPTOR_OPTION_WRITE_BASE_URI:
    case RAPTOR_OPTION_RELATIVE_URIS:
    case RAPTOR_OPTION_PREFIX_ELEMENTS:
      /* actually handled above because value is integral */
      return -1;
      break;

    /* parser options */
    case RAPTOR_OPTION_SCANNING:
    case RAPTOR_OPTION_ALLOW_NON_NS_ATTRIBUTES:
    case RAPTOR_OPTION_ALLOW_OTHER_PARSETYPES:
    case RAPTOR_OPTION_ALLOW_BAGID:
    case RAPTOR_OPTION_ALLOW_RDF_TYPE_RDF_LIST:
    case RAPTOR_OPTION_NORMALIZE_LANGUAGE:
    case RAPTOR_OPTION_NON_NFC_FATAL:
    case RAPTOR_OPTION_WARN_OTHER_PARSETYPES:
    case RAPTOR_OPTION_CHECK_RDF_ID:
    case RAPTOR_OPTION_HTML_TAG_SOUP:
    case RAPTOR_OPTION_MICROFORMATS:
    case RAPTOR_OPTION_HTML_LINK:
    case RAPTOR_OPTION_WWW_TIMEOUT:

    /* Shared */
    case RAPTOR_OPTION_NO_NET:

    /* XML writer options */
    case RAPTOR_OPTION_WRITER_AUTO_INDENT:
    case RAPTOR_OPTION_WRITER_AUTO_EMPTY:
    case RAPTOR_OPTION_WRITER_INDENT_WIDTH:
    case RAPTOR_OPTION_WRITER_XML_VERSION:
    case RAPTOR_OPTION_WRITER_XML_DECLARATION:

    /* GraphViz serializer options */
    case RAPTOR_OPTION_RESOURCE_BORDER:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_resource_border), value);
      break;
    case RAPTOR_OPTION_LITERAL_BORDER:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_literal_border), value);
      break;
    case RAPTOR_OPTION_BNODE_BORDER:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_bnode_border), value);
      break;
    case RAPTOR_OPTION_RESOURCE_FILL:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_resource_fill), value);
      break;
    case RAPTOR_OPTION_LITERAL_FILL:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_literal_fill), value);
      break;
    case RAPTOR_OPTION_BNODE_FILL:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_bnode_fill), value);
      break;

    /* JSON serializer options */
    case RAPTOR_OPTION_JSON_CALLBACK:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_json_callback), value);
      break;

    case RAPTOR_OPTION_JSON_EXTRA_DATA:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_json_extra_data), value);
      break;

    case RAPTOR_OPTION_RSS_TRIPLES:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_rss_triples), value);
      break;

    case RAPTOR_OPTION_ATOM_ENTRY_URI:
      return raptor_serializer_copy_string(
        (unsigned char **)&(serializer->option_atom_entry_uri), value);
      break;

    /* WWW options */
    case RAPTOR_OPTION_WWW_HTTP_CACHE_CONTROL:
    case RAPTOR_OPTION_WWW_HTTP_USER_AGENT:

    default:
      return -1;
      break;
  }

  return 0;
}


/**
 * raptor_serializer_get_option:
 * @serializer: #raptor_serializer serializer object
 * @option: option to get value
 *
 * Get various serializer options.
 * 
 * The allowed options are available via
 * raptor_world_enumerate_serializer_options().
 *
 * Note: no option value is negative
 *
 * Return value: option value or < 0 for an illegal option
 **/
int
raptor_serializer_get_option(raptor_serializer *serializer, 
                              raptor_option option)
{
  int result = -1;
  
  if(!raptor_option_is_valid_for_area(option, RAPTOR_OPTION_AREA_SERIALIZER))
    return -1;

  if(!raptor_option_value_is_numeric(option))
    return -1;

  switch(option) {
    case RAPTOR_OPTION_WRITE_BASE_URI:
      result = (serializer->option_write_base_uri != 0);
      break;

    case RAPTOR_OPTION_RELATIVE_URIS:
      result = (serializer->option_relative_uris != 0);
      break;

    /* String options */
    case RAPTOR_OPTION_RESOURCE_BORDER:
    case RAPTOR_OPTION_LITERAL_BORDER:
    case RAPTOR_OPTION_BNODE_BORDER:
    case RAPTOR_OPTION_RESOURCE_FILL:
    case RAPTOR_OPTION_LITERAL_FILL:
    case RAPTOR_OPTION_BNODE_FILL:
    case RAPTOR_OPTION_JSON_CALLBACK:
    case RAPTOR_OPTION_JSON_EXTRA_DATA:
    case RAPTOR_OPTION_RSS_TRIPLES:
    case RAPTOR_OPTION_ATOM_ENTRY_URI:
      result= -1;
      break;

    case RAPTOR_OPTION_PREFIX_ELEMENTS:
      result = serializer->option_prefix_elements;
      break;
      
    case RAPTOR_OPTION_WRITER_XML_VERSION:
      result = serializer->xml_version;
      break;
  
    case RAPTOR_OPTION_WRITER_XML_DECLARATION:
      result = serializer->option_write_xml_declaration;
      break;
      
    /* parser options */
    case RAPTOR_OPTION_SCANNING:
    case RAPTOR_OPTION_ALLOW_NON_NS_ATTRIBUTES:
    case RAPTOR_OPTION_ALLOW_OTHER_PARSETYPES:
    case RAPTOR_OPTION_ALLOW_BAGID:
    case RAPTOR_OPTION_ALLOW_RDF_TYPE_RDF_LIST:
    case RAPTOR_OPTION_NORMALIZE_LANGUAGE:
    case RAPTOR_OPTION_NON_NFC_FATAL:
    case RAPTOR_OPTION_WARN_OTHER_PARSETYPES:
    case RAPTOR_OPTION_CHECK_RDF_ID:
    case RAPTOR_OPTION_HTML_TAG_SOUP:
    case RAPTOR_OPTION_MICROFORMATS:
    case RAPTOR_OPTION_HTML_LINK:
    case RAPTOR_OPTION_WWW_TIMEOUT:

    /* Shared */
    case RAPTOR_OPTION_NO_NET:

    /* XML writer options */
    case RAPTOR_OPTION_WRITER_AUTO_INDENT:
    case RAPTOR_OPTION_WRITER_AUTO_EMPTY:
    case RAPTOR_OPTION_WRITER_INDENT_WIDTH:

    /* WWW options */
    case RAPTOR_OPTION_WWW_HTTP_CACHE_CONTROL:
    case RAPTOR_OPTION_WWW_HTTP_USER_AGENT:

    default:
      break;
  }
  
  return result;
}


/**
 * raptor_serializer_get_option_string:
 * @serializer: #raptor_serializer serializer object
 * @option: option to get value
 *
 * Get serializer options with string values.
 * 
 * The allowed options are available via
 * raptor_world_enumerate_serializer_options().
 *
 * Return value: option value or NULL for an illegal option or no value
 **/
const unsigned char *
raptor_serializer_get_option_string(raptor_serializer *serializer, 
                                     raptor_option option)
{
  if(!raptor_option_is_valid_for_area(option, RAPTOR_OPTION_AREA_SERIALIZER))
    return NULL;

  if(raptor_option_value_is_numeric(option))
    return NULL;
  
  switch(option) {
    case RAPTOR_OPTION_WRITE_BASE_URI:
    case RAPTOR_OPTION_RELATIVE_URIS:
      /* actually handled above because value is integral */
      return NULL;
      break;
      
    /* GraphViz serializer options */
    case RAPTOR_OPTION_RESOURCE_BORDER:
      return (unsigned char *)(serializer->option_resource_border);
      break;
    case RAPTOR_OPTION_LITERAL_BORDER:
      return (unsigned char *)(serializer->option_literal_border);
      break;
    case RAPTOR_OPTION_BNODE_BORDER:
      return (unsigned char *)(serializer->option_bnode_border);
      break;
    case RAPTOR_OPTION_RESOURCE_FILL:
      return (unsigned char *)(serializer->option_resource_fill);
      break;
    case RAPTOR_OPTION_LITERAL_FILL:
      return (unsigned char *)(serializer->option_literal_fill);
      break;
    case RAPTOR_OPTION_BNODE_FILL:
      return (unsigned char *)(serializer->option_bnode_fill);
      break;
    case RAPTOR_OPTION_JSON_CALLBACK:
      return (unsigned char *)(serializer->option_json_callback);
      break;
    case RAPTOR_OPTION_JSON_EXTRA_DATA:
      return (unsigned char *)(serializer->option_json_extra_data);
      break;
    case RAPTOR_OPTION_RSS_TRIPLES:
      return (unsigned char *)(serializer->option_rss_triples);
      break;
    case RAPTOR_OPTION_ATOM_ENTRY_URI:
      return (unsigned char *)(serializer->option_atom_entry_uri);
      break;
    case RAPTOR_OPTION_PREFIX_ELEMENTS:
      return NULL;
      break;
        
    /* parser options */
    case RAPTOR_OPTION_SCANNING:
    case RAPTOR_OPTION_ALLOW_NON_NS_ATTRIBUTES:
    case RAPTOR_OPTION_ALLOW_OTHER_PARSETYPES:
    case RAPTOR_OPTION_ALLOW_BAGID:
    case RAPTOR_OPTION_ALLOW_RDF_TYPE_RDF_LIST:
    case RAPTOR_OPTION_NORMALIZE_LANGUAGE:
    case RAPTOR_OPTION_NON_NFC_FATAL:
    case RAPTOR_OPTION_WARN_OTHER_PARSETYPES:
    case RAPTOR_OPTION_CHECK_RDF_ID:
    case RAPTOR_OPTION_HTML_TAG_SOUP:
    case RAPTOR_OPTION_MICROFORMATS:
    case RAPTOR_OPTION_HTML_LINK:
    case RAPTOR_OPTION_WWW_TIMEOUT:

    /* Shared */
    case RAPTOR_OPTION_NO_NET:

    /* XML writer options */
    case RAPTOR_OPTION_WRITER_AUTO_INDENT:
    case RAPTOR_OPTION_WRITER_AUTO_EMPTY:
    case RAPTOR_OPTION_WRITER_INDENT_WIDTH:
    case RAPTOR_OPTION_WRITER_XML_VERSION:
    case RAPTOR_OPTION_WRITER_XML_DECLARATION:

    /* WWW options */
    case RAPTOR_OPTION_WWW_HTTP_CACHE_CONTROL:
    case RAPTOR_OPTION_WWW_HTTP_USER_AGENT:

    default:
      return NULL;
      break;
  }

  return NULL;
}


/**
 * raptor_serializer_get_locator:
 * @rdf_serializer: raptor serializer
 *
 * Get the serializer raptor locator object.
 * 
 * Return value: raptor locator
 **/
raptor_locator*
raptor_serializer_get_locator(raptor_serializer *rdf_serializer)
{
  return &rdf_serializer->locator;
}


/**
 * raptor_serializer_get_world:
 * @rdf_serializer: raptor serializer
 * 
 * Get the #raptor_world object associated with a serializer.
 *
 * Return value: raptor_world* pointer
 **/
raptor_world *
raptor_serializer_get_world(raptor_serializer* rdf_serializer)
{
  return rdf_serializer->world;
}
