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
#include "qapi-visit-misc-target.h"

#if defined(TARGET_ALPHA) || defined(TARGET_ARM) || defined(TARGET_HPPA) || defined(TARGET_I386) || defined(TARGET_MIPS) || defined(TARGET_MIPS64) || defined(TARGET_MOXIE) || defined(TARGET_PPC) || defined(TARGET_PPC64) || defined(TARGET_S390X) || defined(TARGET_SH4) || defined(TARGET_SPARC)
void visit_type_q_obj_RTC_CHANGE_arg_members(Visitor *v, q_obj_RTC_CHANGE_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "offset", &obj->offset, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_ALPHA) || defined(TARGET_ARM) || defined(TARGET_HPPA) || defined(TARGET_I386) || defined(TARGET_MIPS) || defined(TARGET_MIPS64) || defined(TARGET_MOXIE) || defined(TARGET_PPC) || defined(TARGET_PPC64) || defined(TARGET_S390X) || defined(TARGET_SH4) || defined(TARGET_SPARC) */

#if defined(TARGET_I386)
void visit_type_SevState(Visitor *v, const char *name, SevState *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &SevState_lookup, errp);
    *obj = value;
}
#endif /* defined(TARGET_I386) */

#if defined(TARGET_I386)
void visit_type_SevInfo_members(Visitor *v, SevInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_bool(v, "enabled", &obj->enabled, &err);
    if (err) {
        goto out;
    }
    visit_type_uint8(v, "api-major", &obj->api_major, &err);
    if (err) {
        goto out;
    }
    visit_type_uint8(v, "api-minor", &obj->api_minor, &err);
    if (err) {
        goto out;
    }
    visit_type_uint8(v, "build-id", &obj->build_id, &err);
    if (err) {
        goto out;
    }
    visit_type_uint32(v, "policy", &obj->policy, &err);
    if (err) {
        goto out;
    }
    visit_type_SevState(v, "state", &obj->state, &err);
    if (err) {
        goto out;
    }
    visit_type_uint32(v, "handle", &obj->handle, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_SevInfo(Visitor *v, const char *name, SevInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(SevInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_SevInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_SevInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_I386) */

#if defined(TARGET_I386)
void visit_type_SevLaunchMeasureInfo_members(Visitor *v, SevLaunchMeasureInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "data", &obj->data, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_SevLaunchMeasureInfo(Visitor *v, const char *name, SevLaunchMeasureInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(SevLaunchMeasureInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_SevLaunchMeasureInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_SevLaunchMeasureInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_I386) */

#if defined(TARGET_I386)
void visit_type_SevCapability_members(Visitor *v, SevCapability *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "pdh", &obj->pdh, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "cert-chain", &obj->cert_chain, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "cbitpos", &obj->cbitpos, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "reduced-phys-bits", &obj->reduced_phys_bits, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_SevCapability(Visitor *v, const char *name, SevCapability **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(SevCapability), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_SevCapability_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_SevCapability(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_I386) */

#if defined(TARGET_S390X)
void visit_type_q_obj_dump_skeys_arg_members(Visitor *v, q_obj_dump_skeys_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "filename", &obj->filename, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_S390X) */

#if defined(TARGET_ARM)
void visit_type_GICCapability_members(Visitor *v, GICCapability *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "version", &obj->version, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "emulated", &obj->emulated, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "kernel", &obj->kernel, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_GICCapability(Visitor *v, const char *name, GICCapability **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(GICCapability), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_GICCapability_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_GICCapability(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_ARM) */

#if defined(TARGET_ARM)
void visit_type_GICCapabilityList(Visitor *v, const char *name, GICCapabilityList **obj, Error **errp)
{
    Error *err = NULL;
    GICCapabilityList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (GICCapabilityList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_GICCapability(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_GICCapabilityList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_ARM) */

/* Dummy declaration to prevent empty .o file */
char qapi_dummy_qapi_visit_misc_target_c;
