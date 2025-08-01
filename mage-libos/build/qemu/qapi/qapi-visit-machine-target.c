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
#include "qapi-visit-machine-target.h"

void visit_type_CpuModelInfo_members(Visitor *v, CpuModelInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "name", &obj->name, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "props", &obj->has_props)) {
        visit_type_any(v, "props", &obj->props, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuModelInfo(Visitor *v, const char *name, CpuModelInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuModelInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuModelInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuModelInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuModelExpansionType(Visitor *v, const char *name, CpuModelExpansionType *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &CpuModelExpansionType_lookup, errp);
    *obj = value;
}

void visit_type_CpuModelCompareResult(Visitor *v, const char *name, CpuModelCompareResult *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &CpuModelCompareResult_lookup, errp);
    *obj = value;
}

#if defined(TARGET_S390X)
void visit_type_CpuModelBaselineInfo_members(Visitor *v, CpuModelBaselineInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuModelInfo(v, "model", &obj->model, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuModelBaselineInfo(Visitor *v, const char *name, CpuModelBaselineInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuModelBaselineInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuModelBaselineInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuModelBaselineInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_S390X) */

#if defined(TARGET_S390X)
void visit_type_CpuModelCompareInfo_members(Visitor *v, CpuModelCompareInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuModelCompareResult(v, "result", &obj->result, &err);
    if (err) {
        goto out;
    }
    visit_type_strList(v, "responsible-properties", &obj->responsible_properties, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuModelCompareInfo(Visitor *v, const char *name, CpuModelCompareInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuModelCompareInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuModelCompareInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuModelCompareInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_S390X) */

#if defined(TARGET_S390X)
void visit_type_q_obj_query_cpu_model_comparison_arg_members(Visitor *v, q_obj_query_cpu_model_comparison_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuModelInfo(v, "modela", &obj->modela, &err);
    if (err) {
        goto out;
    }
    visit_type_CpuModelInfo(v, "modelb", &obj->modelb, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_S390X) */

#if defined(TARGET_S390X)
void visit_type_q_obj_query_cpu_model_baseline_arg_members(Visitor *v, q_obj_query_cpu_model_baseline_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuModelInfo(v, "modela", &obj->modela, &err);
    if (err) {
        goto out;
    }
    visit_type_CpuModelInfo(v, "modelb", &obj->modelb, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_S390X) */

#if defined(TARGET_S390X) || defined(TARGET_I386) || defined(TARGET_ARM)
void visit_type_CpuModelExpansionInfo_members(Visitor *v, CpuModelExpansionInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuModelInfo(v, "model", &obj->model, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuModelExpansionInfo(Visitor *v, const char *name, CpuModelExpansionInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuModelExpansionInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuModelExpansionInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuModelExpansionInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_S390X) || defined(TARGET_I386) || defined(TARGET_ARM) */

#if defined(TARGET_S390X) || defined(TARGET_I386) || defined(TARGET_ARM)
void visit_type_q_obj_query_cpu_model_expansion_arg_members(Visitor *v, q_obj_query_cpu_model_expansion_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuModelExpansionType(v, "type", &obj->type, &err);
    if (err) {
        goto out;
    }
    visit_type_CpuModelInfo(v, "model", &obj->model, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_S390X) || defined(TARGET_I386) || defined(TARGET_ARM) */

#if defined(TARGET_PPC) || defined(TARGET_ARM) || defined(TARGET_I386) || defined(TARGET_S390X) || defined(TARGET_MIPS)
void visit_type_CpuDefinitionInfo_members(Visitor *v, CpuDefinitionInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "name", &obj->name, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "migration-safe", &obj->has_migration_safe)) {
        visit_type_bool(v, "migration-safe", &obj->migration_safe, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_bool(v, "static", &obj->q_static, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "unavailable-features", &obj->has_unavailable_features)) {
        visit_type_strList(v, "unavailable-features", &obj->unavailable_features, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_str(v, "typename", &obj->q_typename, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "alias-of", &obj->has_alias_of)) {
        visit_type_str(v, "alias-of", &obj->alias_of, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuDefinitionInfo(Visitor *v, const char *name, CpuDefinitionInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuDefinitionInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuDefinitionInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuDefinitionInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_PPC) || defined(TARGET_ARM) || defined(TARGET_I386) || defined(TARGET_S390X) || defined(TARGET_MIPS) */

#if defined(TARGET_PPC) || defined(TARGET_ARM) || defined(TARGET_I386) || defined(TARGET_S390X) || defined(TARGET_MIPS)
void visit_type_CpuDefinitionInfoList(Visitor *v, const char *name, CpuDefinitionInfoList **obj, Error **errp)
{
    Error *err = NULL;
    CpuDefinitionInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (CpuDefinitionInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_CpuDefinitionInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuDefinitionInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}
#endif /* defined(TARGET_PPC) || defined(TARGET_ARM) || defined(TARGET_I386) || defined(TARGET_S390X) || defined(TARGET_MIPS) */

/* Dummy declaration to prevent empty .o file */
char qapi_dummy_qapi_visit_machine_target_c;
