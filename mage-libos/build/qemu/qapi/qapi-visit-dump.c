/* AUTOMATICALLY GENERATED, DO NOT MODIFY */

/*
 * Schema-defined QAPI visitors
 *
 * Copyright IBM, Corp. 2011
 * Copyright (C) 2014-2018 Red Hat, Inc.
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/qmp/qerror.h"
#include "qapi-visit-dump.h"

void visit_type_DumpGuestMemoryFormat(Visitor *v, const char *name, DumpGuestMemoryFormat *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &DumpGuestMemoryFormat_lookup, errp);
    *obj = value;
}

void visit_type_q_obj_dump_guest_memory_arg_members(Visitor *v, q_obj_dump_guest_memory_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_bool(v, "paging", &obj->paging, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "protocol", &obj->protocol, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "detach", &obj->has_detach)) {
        visit_type_bool(v, "detach", &obj->detach, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "begin", &obj->has_begin)) {
        visit_type_int(v, "begin", &obj->begin, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "length", &obj->has_length)) {
        visit_type_int(v, "length", &obj->length, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "format", &obj->has_format)) {
        visit_type_DumpGuestMemoryFormat(v, "format", &obj->format, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_DumpStatus(Visitor *v, const char *name, DumpStatus *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &DumpStatus_lookup, errp);
    *obj = value;
}

void visit_type_DumpQueryResult_members(Visitor *v, DumpQueryResult *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_DumpStatus(v, "status", &obj->status, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "completed", &obj->completed, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "total", &obj->total, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_DumpQueryResult(Visitor *v, const char *name, DumpQueryResult **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(DumpQueryResult), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_DumpQueryResult_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_DumpQueryResult(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_DUMP_COMPLETED_arg_members(Visitor *v, q_obj_DUMP_COMPLETED_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_DumpQueryResult(v, "result", &obj->result, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "error", &obj->has_error)) {
        visit_type_str(v, "error", &obj->error, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_DumpGuestMemoryFormatList(Visitor *v, const char *name, DumpGuestMemoryFormatList **obj, Error **errp)
{
    Error *err = NULL;
    DumpGuestMemoryFormatList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (DumpGuestMemoryFormatList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_DumpGuestMemoryFormat(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_DumpGuestMemoryFormatList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_DumpGuestMemoryCapability_members(Visitor *v, DumpGuestMemoryCapability *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_DumpGuestMemoryFormatList(v, "formats", &obj->formats, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_DumpGuestMemoryCapability(Visitor *v, const char *name, DumpGuestMemoryCapability **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(DumpGuestMemoryCapability), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_DumpGuestMemoryCapability_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_DumpGuestMemoryCapability(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

/* Dummy declaration to prevent empty .o file */
char qapi_dummy_qapi_visit_dump_c;
