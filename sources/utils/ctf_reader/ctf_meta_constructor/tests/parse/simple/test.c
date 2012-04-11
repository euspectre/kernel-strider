#include "test_common.h"

#include "ctf_meta.h"

#include <stdio.h>
#include <assert.h>

#include <malloc.h> /* malloc */
#include <string.h> /* memcpy, strcmp */

#define checked(op) assert(op)

/* Size of dynamic scopes in bytes(according to metadata) */
#define PACKET_HEADER_SIZE 8
#define PACKET_CONTEXT_SIZE 2
#define STREAM_EVENT_HEADER_SIZE 10
#define EVENT_CONTEXT_SIZE 2
#define EVENT_FIELDS_SIZE 16

static int test_meta(struct ctf_meta* meta);

int main(int argc, char** argv)
{
    assert(argc == 2);
    struct ctf_meta* meta = ctf_meta_create_from_file(argv[1]);
    
    if(meta == NULL)
    {
        printf("Failed to create CTF meta from file.\n");
        return 1;
    }
    
    int result = ctf_meta_instantiate(meta);
    if(result != 0)
    {
        printf("Failed to instantiate CTF metadata.\n");
        ctf_meta_destroy(meta);
        return 1;
    }
    
    result = test_meta(meta);
    
    ctf_meta_destroy(meta);
    
    return result;
}




static int32_t get_field1(const void* map)
{
    const char* bytes = map;
    
    return (((int32_t)bytes[0]) << 8)
        + bytes[1];
}

static int32_t get_field2(const void* map)
{
    const char* bytes = map;
    
    return (((int32_t)bytes[4]) << 24)
        + (((int32_t)bytes[5]) << 16)
        + (((int32_t)bytes[6]) << 8)
        + bytes[7];
}

static int32_t get_stream_type_val(const void* map)
{
    const char* bytes = map;
    
    return (((int32_t)bytes[0]) << 8)
        + bytes[1];
}

static int32_t get_event_header_id(const void* map, int index)
{
    const char* bytes = map;
    
    return (((int32_t)bytes[index * 2]) << 8)
        + bytes[index * 2 + 1];
}


static int32_t get_event_type_very_complex(const void* map)
{
    const char* bytes = map;
    
    return (((int32_t)bytes[0]) << 8)
        + bytes[1];
}

static int32_t get_event_fields_n(const void* map)
{
    const char* bytes = map;
    
    return bytes[0];
}


static int32_t get_event_fields_value(const void* map, int index)
{
    const char* bytes = map;
    
    return (((int32_t)bytes[index * 2 + 2]) << 8)
        + bytes[index * 2 + 3];
}

int test_meta(struct ctf_meta* meta)
{
    /******* Check packet header **********/
    char* packet_header_map = malloc(PACKET_HEADER_SIZE);
    assert(packet_header_map);
    memcpy(packet_header_map, "packet_header_map", PACKET_HEADER_SIZE);
    
    struct test_context_info_static context_info_static_packet_header;
    test_context_info_static_init(&context_info_static_packet_header,
        packet_header_map, 0, PACKET_HEADER_SIZE * 8);
    
    struct ctf_var* packet_header = ctf_meta_find_var(meta,
        "trace.packet.header");
    assert(packet_header != NULL);
    
    struct ctf_var* field1 = ctf_meta_find_var(meta, "trace.packet.header.field1");
    assert(field1 != NULL);
    
    struct ctf_var* field2 = ctf_meta_find_var(meta, "trace.packet.header.field2");
    assert(field2 != NULL);
    
    struct ctf_context* context_packet_header = ctf_meta_create_context(
        meta, packet_header, &context_info_static_packet_header.base, NULL);
    assert(context_packet_header != NULL);
    
    assert(ctf_var_is_exist(field1, context_packet_header) == 1);
    assert(ctf_var_is_exist(field2, context_packet_header) == 1);
    
    checked(ctf_var_get_map(field1, context_packet_header, NULL));
    
    int32_t field1_val = ctf_var_get_int32(field1, context_packet_header);
    int32_t field1_val_expected = get_field1(packet_header_map);
    
    if(field1_val != field1_val_expected)
    {
        printf("Expected, that value of field1 would be %d, but it is %d.\n",
            (int)field1_val_expected, (int)field1_val);
        goto err_packet_header;
    }
    
    checked(ctf_var_get_map(field2, context_packet_header, NULL));
    
    int32_t field2_val = ctf_var_get_int32(field2, context_packet_header);
    int32_t field2_val_expected = get_field2(packet_header_map);
    
    if(field2_val != field2_val_expected)
    {
        printf("Expected, that value of field2 would be %d, but it is %d.\n",
            (int)field2_val_expected, (int)field2_val);
        goto err_packet_header;
    }
    /******* Check packet context **********/
    char* packet_context_map = malloc(PACKET_CONTEXT_SIZE);
    assert(packet_context_map);
    
    packet_context_map[0] = 0;
    packet_context_map[1] = 6;
    const char* packet_context_enum_expected = "very_complex";
       
    struct test_context_info_static context_info_static_packet_context;
    test_context_info_static_init(&context_info_static_packet_context,
        packet_context_map, 0, PACKET_CONTEXT_SIZE * 8);
    
    struct ctf_var* packet_context = ctf_meta_find_var(meta,
        "stream.packet.context");
    assert(packet_context != NULL);
    
    struct ctf_context* context_packet_context = ctf_meta_create_context(
        meta, packet_context, &context_info_static_packet_context.base,
        context_packet_header);
    assert(context_packet_context != NULL);
    
    checked(ctf_var_get_map(packet_context, context_packet_context, NULL));
    
    int32_t packet_context_val = ctf_var_get_int32(packet_context, context_packet_context);
    int32_t packet_context_val_expected = get_stream_type_val(packet_context_map);
    
    if(packet_context_val != packet_context_val_expected)
    {
        printf("Expected, that value of stream_type would be %d, but it is %d.\n",
            (int)packet_context_val_expected, (int)packet_context_val);
        goto err_packet_context;
    }

    
    const char* packet_context_enum = ctf_var_get_enum(packet_context, context_packet_context);
    
    if((packet_context_enum == NULL)
        || (strcmp(packet_context_enum, packet_context_enum_expected) != 0))
    {
        printf("Expected, that enumeration value of stream_type would "
            "be '%s'(integer value is %d), but it is '%s'.\n",
            packet_context_enum_expected,
            packet_context_val,
            packet_context_enum);
        goto err_packet_context;
    }
    
    /******* Check stream event header **********/
    char* stream_event_header_map = malloc(STREAM_EVENT_HEADER_SIZE);
    assert(stream_event_header_map);
    
    memcpy(stream_event_header_map, "streameven", STREAM_EVENT_HEADER_SIZE);
    int elem_index = 4;
    
    struct test_context_info_static context_info_static_stream_event_header;
    test_context_info_static_init(&context_info_static_stream_event_header,
        stream_event_header_map, 0, STREAM_EVENT_HEADER_SIZE * 8);
    
    struct ctf_var* stream_event_header = ctf_meta_find_var(meta,
        "stream.event.header");
    assert(stream_event_header != NULL);
    
    struct ctf_context* context_stream_event_header =
        ctf_meta_create_context(meta,
            stream_event_header,
            &context_info_static_stream_event_header.base,
            context_packet_context);
    assert(context_stream_event_header != NULL);
    
    struct ctf_var* stream_event_header_id =
        ctf_var_find_var(stream_event_header, "id[]");
    assert(stream_event_header_id != NULL);

    struct ctf_context* context_stream_event_header_id =
        ctf_var_elem_create_context(stream_event_header_id,
            context_stream_event_header,
            elem_index);
    assert(context_stream_event_header_id != NULL);

    checked(ctf_var_get_map(stream_event_header_id,
        context_stream_event_header_id, NULL));
    
    int32_t stream_event_header_id_val = ctf_var_get_int32(
        stream_event_header_id, context_stream_event_header_id);
    int32_t stream_event_header_id_val_expected =
        get_event_header_id(stream_event_header_map, elem_index);
    
    if(stream_event_header_id_val != stream_event_header_id_val_expected)
    {
        printf("Expected, that %d-th element of event id would be %d, "
            "but it is %d.\n",
            elem_index,
            (int)stream_event_header_id_val_expected,
            (int)stream_event_header_id_val);
        goto err_stream_event_header;
    }

    /******* Check event context **********/
    char* event_context_map = malloc(EVENT_CONTEXT_SIZE);
    assert(event_context_map);
    
    memcpy(event_context_map, "\001\002", EVENT_CONTEXT_SIZE);
    
    struct test_context_info_static context_info_static_event_context;
    test_context_info_static_init(&context_info_static_event_context,
        event_context_map, 0, PACKET_CONTEXT_SIZE * 8);
    
    struct ctf_var* event_context = ctf_meta_find_var(meta,
        "event.context");
    assert(event_context != NULL);
    
    struct ctf_context* context_event_context = ctf_meta_create_context(
        meta, event_context,
        &context_info_static_event_context.base,
        context_stream_event_header);
    assert(event_context != NULL);
    
    struct ctf_var* event_very_complex_type = ctf_var_find_var(event_context,
        "very_complex");
    assert(event_very_complex_type);
    
    assert(ctf_var_is_exist(event_very_complex_type, context_event_context) == 1);
    checked(ctf_var_get_map(event_very_complex_type, context_event_context, NULL));
    
    int32_t event_very_complex_type_val =
        ctf_var_get_int32(event_very_complex_type, context_event_context);
    int32_t event_very_complex_type_val_expected =
        get_event_type_very_complex(event_context_map);
    
    if(event_very_complex_type_val != event_very_complex_type_val_expected)
    {
        printf("Expected, that value of event context would be %d, but it is %d.\n",
            (int)event_very_complex_type_val_expected,
            (int)event_very_complex_type_val);
        goto err_event_context;
    }

    /************ Check event fields **********/
    char* event_fields_map = malloc(EVENT_FIELDS_SIZE);
    assert(event_fields_map);
    
    memcpy(event_fields_map, "\007eventfieldseven", EVENT_FIELDS_SIZE);
    int value_index = 3;
    
    struct test_context_info_static context_info_static_event_fields;
    test_context_info_static_init(&context_info_static_event_fields,
        event_fields_map, 0, EVENT_FIELDS_SIZE * 8);
    
    struct ctf_var* event_fields = ctf_meta_find_var(meta,
        "event.fields");
    assert(event_fields != NULL);
    
    struct ctf_context* context_event_fields =
        ctf_meta_create_context(meta,
            event_fields,
            &context_info_static_event_fields.base,
            context_event_context);
    assert(context_event_fields != NULL);
    
    struct ctf_var* event_fields_values =
        ctf_var_find_var(event_fields, "values");
    assert(event_fields_values != NULL);

    int32_t event_fields_values_n = ctf_var_get_n_elems(
        event_fields_values, context_event_fields);
    int32_t event_fields_values_n_expected = get_event_fields_n(
        event_fields_map);
    if(event_fields_values_n != event_fields_values_n_expected)
    {
        printf("Expected, that size of 'values' sequence would be %d "
            "but it is %d.\n",
            (int)event_fields_values_n_expected,
            (int)event_fields_values_n);
        goto err_event_fields_n;
    }
    
    struct ctf_var* event_fields_value =
        ctf_var_find_var(event_fields, "values[]");
    assert(event_fields_value != NULL);

    struct ctf_context* context_event_fields_value =
        ctf_var_elem_create_context(event_fields_value,
            context_event_fields,
            value_index);
    assert(context_event_fields_value != NULL);

    checked(ctf_var_get_map(event_fields_value,
        context_event_fields_value, NULL));
    
    int32_t event_fields_value_val = ctf_var_get_int32(
        event_fields_value, context_event_fields_value);
    int32_t event_fields_value_val_expected =
        get_event_fields_value(event_fields_map, value_index);
    
    if(event_fields_value_val != event_fields_value_val_expected)
    {
        printf("Expected, that %d-th element of event value would be %d, "
            "but it is %d.\n",
            value_index,
            (int)event_fields_value_val_expected,
            (int)event_fields_value_val);
        goto err_event_fields;
    }

    ctf_context_destroy(context_event_fields_value);
    ctf_context_destroy(context_event_fields);
    free(event_fields_map);

    ctf_context_destroy(context_event_context);
    free(event_context_map);

    ctf_context_destroy(context_stream_event_header_id);
    ctf_context_destroy(context_stream_event_header);
    free(stream_event_header_map);

    ctf_context_destroy(context_packet_context);
    free(packet_context_map);

    ctf_context_destroy(context_packet_header);
    free(packet_header_map);
   
    return 0;

err_event_fields:
    ctf_context_destroy(context_event_fields_value);
err_event_fields_n:
    ctf_context_destroy(context_event_fields);
    free(event_fields_map);
err_event_context:
    ctf_context_destroy(context_event_context);
    free(event_context_map);
err_stream_event_header:
    ctf_context_destroy(context_stream_event_header_id);
    ctf_context_destroy(context_stream_event_header);
    free(stream_event_header_map);
err_packet_context:
    ctf_context_destroy(context_packet_context);
    free(packet_context_map);
err_packet_header:
    ctf_context_destroy(context_packet_header);
    free(packet_header_map);
    return 1;
}