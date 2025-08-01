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
#include "qapi-visit-misc.h"

void visit_type_QMPCapabilityList(Visitor *v, const char *name, QMPCapabilityList **obj, Error **errp)
{
    Error *err = NULL;
    QMPCapabilityList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (QMPCapabilityList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_QMPCapability(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_QMPCapabilityList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_qmp_capabilities_arg_members(Visitor *v, q_obj_qmp_capabilities_arg *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "enable", &obj->has_enable)) {
        visit_type_QMPCapabilityList(v, "enable", &obj->enable, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_QMPCapability(Visitor *v, const char *name, QMPCapability *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &QMPCapability_lookup, errp);
    *obj = value;
}

void visit_type_VersionTriple_members(Visitor *v, VersionTriple *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "major", &obj->major, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "minor", &obj->minor, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "micro", &obj->micro, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_VersionTriple(Visitor *v, const char *name, VersionTriple **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(VersionTriple), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_VersionTriple_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_VersionTriple(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_VersionInfo_members(Visitor *v, VersionInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_VersionTriple(v, "qemu", &obj->qemu, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "package", &obj->package, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_VersionInfo(Visitor *v, const char *name, VersionInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(VersionInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_VersionInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_VersionInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CommandInfo_members(Visitor *v, CommandInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "name", &obj->name, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CommandInfo(Visitor *v, const char *name, CommandInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CommandInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CommandInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CommandInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CommandInfoList(Visitor *v, const char *name, CommandInfoList **obj, Error **errp)
{
    Error *err = NULL;
    CommandInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (CommandInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_CommandInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CommandInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_LostTickPolicy(Visitor *v, const char *name, LostTickPolicy *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &LostTickPolicy_lookup, errp);
    *obj = value;
}

void visit_type_q_obj_add_client_arg_members(Visitor *v, q_obj_add_client_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "protocol", &obj->protocol, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "fdname", &obj->fdname, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "skipauth", &obj->has_skipauth)) {
        visit_type_bool(v, "skipauth", &obj->skipauth, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "tls", &obj->has_tls)) {
        visit_type_bool(v, "tls", &obj->tls, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_NameInfo_members(Visitor *v, NameInfo *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "name", &obj->has_name)) {
        visit_type_str(v, "name", &obj->name, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_NameInfo(Visitor *v, const char *name, NameInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(NameInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_NameInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_NameInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_KvmInfo_members(Visitor *v, KvmInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_bool(v, "enabled", &obj->enabled, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "present", &obj->present, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_KvmInfo(Visitor *v, const char *name, KvmInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(KvmInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_KvmInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_KvmInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_UuidInfo_members(Visitor *v, UuidInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "UUID", &obj->UUID, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_UuidInfo(Visitor *v, const char *name, UuidInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(UuidInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_UuidInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_UuidInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_EventInfo_members(Visitor *v, EventInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "name", &obj->name, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_EventInfo(Visitor *v, const char *name, EventInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(EventInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_EventInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_EventInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_EventInfoList(Visitor *v, const char *name, EventInfoList **obj, Error **errp)
{
    Error *err = NULL;
    EventInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (EventInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_EventInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_EventInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_IOThreadInfo_members(Visitor *v, IOThreadInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "id", &obj->id, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "thread-id", &obj->thread_id, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "poll-max-ns", &obj->poll_max_ns, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "poll-grow", &obj->poll_grow, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "poll-shrink", &obj->poll_shrink, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_IOThreadInfo(Visitor *v, const char *name, IOThreadInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(IOThreadInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_IOThreadInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_IOThreadInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_IOThreadInfoList(Visitor *v, const char *name, IOThreadInfoList **obj, Error **errp)
{
    Error *err = NULL;
    IOThreadInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (IOThreadInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_IOThreadInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_IOThreadInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_BalloonInfo_members(Visitor *v, BalloonInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "actual", &obj->actual, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_BalloonInfo(Visitor *v, const char *name, BalloonInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(BalloonInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_BalloonInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_BalloonInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_BALLOON_CHANGE_arg_members(Visitor *v, q_obj_BALLOON_CHANGE_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "actual", &obj->actual, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciMemoryRange_members(Visitor *v, PciMemoryRange *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "base", &obj->base, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "limit", &obj->limit, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciMemoryRange(Visitor *v, const char *name, PciMemoryRange **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciMemoryRange), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciMemoryRange_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciMemoryRange(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciMemoryRegion_members(Visitor *v, PciMemoryRegion *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "bar", &obj->bar, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "type", &obj->type, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "address", &obj->address, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "size", &obj->size, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "prefetch", &obj->has_prefetch)) {
        visit_type_bool(v, "prefetch", &obj->prefetch, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "mem_type_64", &obj->has_mem_type_64)) {
        visit_type_bool(v, "mem_type_64", &obj->mem_type_64, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciMemoryRegion(Visitor *v, const char *name, PciMemoryRegion **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciMemoryRegion), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciMemoryRegion_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciMemoryRegion(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciBusInfo_members(Visitor *v, PciBusInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "number", &obj->number, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "secondary", &obj->secondary, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "subordinate", &obj->subordinate, &err);
    if (err) {
        goto out;
    }
    visit_type_PciMemoryRange(v, "io_range", &obj->io_range, &err);
    if (err) {
        goto out;
    }
    visit_type_PciMemoryRange(v, "memory_range", &obj->memory_range, &err);
    if (err) {
        goto out;
    }
    visit_type_PciMemoryRange(v, "prefetchable_range", &obj->prefetchable_range, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciBusInfo(Visitor *v, const char *name, PciBusInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciBusInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciBusInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciBusInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciDeviceInfoList(Visitor *v, const char *name, PciDeviceInfoList **obj, Error **errp)
{
    Error *err = NULL;
    PciDeviceInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (PciDeviceInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_PciDeviceInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciDeviceInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciBridgeInfo_members(Visitor *v, PciBridgeInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_PciBusInfo(v, "bus", &obj->bus, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "devices", &obj->has_devices)) {
        visit_type_PciDeviceInfoList(v, "devices", &obj->devices, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciBridgeInfo(Visitor *v, const char *name, PciBridgeInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciBridgeInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciBridgeInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciBridgeInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciDeviceClass_members(Visitor *v, PciDeviceClass *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "desc", &obj->has_desc)) {
        visit_type_str(v, "desc", &obj->desc, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_int(v, "class", &obj->q_class, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciDeviceClass(Visitor *v, const char *name, PciDeviceClass **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciDeviceClass), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciDeviceClass_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciDeviceClass(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciDeviceId_members(Visitor *v, PciDeviceId *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "device", &obj->device, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "vendor", &obj->vendor, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "subsystem", &obj->has_subsystem)) {
        visit_type_int(v, "subsystem", &obj->subsystem, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "subsystem-vendor", &obj->has_subsystem_vendor)) {
        visit_type_int(v, "subsystem-vendor", &obj->subsystem_vendor, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciDeviceId(Visitor *v, const char *name, PciDeviceId **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciDeviceId), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciDeviceId_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciDeviceId(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciMemoryRegionList(Visitor *v, const char *name, PciMemoryRegionList **obj, Error **errp)
{
    Error *err = NULL;
    PciMemoryRegionList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (PciMemoryRegionList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_PciMemoryRegion(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciMemoryRegionList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciDeviceInfo_members(Visitor *v, PciDeviceInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "bus", &obj->bus, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "slot", &obj->slot, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "function", &obj->function, &err);
    if (err) {
        goto out;
    }
    visit_type_PciDeviceClass(v, "class_info", &obj->class_info, &err);
    if (err) {
        goto out;
    }
    visit_type_PciDeviceId(v, "id", &obj->id, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "irq", &obj->has_irq)) {
        visit_type_int(v, "irq", &obj->irq, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_str(v, "qdev_id", &obj->qdev_id, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "pci_bridge", &obj->has_pci_bridge)) {
        visit_type_PciBridgeInfo(v, "pci_bridge", &obj->pci_bridge, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_PciMemoryRegionList(v, "regions", &obj->regions, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciDeviceInfo(Visitor *v, const char *name, PciDeviceInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciDeviceInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciDeviceInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciDeviceInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciInfo_members(Visitor *v, PciInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "bus", &obj->bus, &err);
    if (err) {
        goto out;
    }
    visit_type_PciDeviceInfoList(v, "devices", &obj->devices, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_PciInfo(Visitor *v, const char *name, PciInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PciInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PciInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PciInfoList(Visitor *v, const char *name, PciInfoList **obj, Error **errp)
{
    Error *err = NULL;
    PciInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (PciInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_PciInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PciInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_memsave_arg_members(Visitor *v, q_obj_memsave_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "val", &obj->val, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "size", &obj->size, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "filename", &obj->filename, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "cpu-index", &obj->has_cpu_index)) {
        visit_type_int(v, "cpu-index", &obj->cpu_index, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_pmemsave_arg_members(Visitor *v, q_obj_pmemsave_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "val", &obj->val, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "size", &obj->size, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "filename", &obj->filename, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_balloon_arg_members(Visitor *v, q_obj_balloon_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "value", &obj->value, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_human_monitor_command_arg_members(Visitor *v, q_obj_human_monitor_command_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "command-line", &obj->command_line, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "cpu-index", &obj->has_cpu_index)) {
        visit_type_int(v, "cpu-index", &obj->cpu_index, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_change_arg_members(Visitor *v, q_obj_change_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "device", &obj->device, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "target", &obj->target, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "arg", &obj->has_arg)) {
        visit_type_str(v, "arg", &obj->arg, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_xen_set_global_dirty_log_arg_members(Visitor *v, q_obj_xen_set_global_dirty_log_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_bool(v, "enable", &obj->enable, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_getfd_arg_members(Visitor *v, q_obj_getfd_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "fdname", &obj->fdname, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_closefd_arg_members(Visitor *v, q_obj_closefd_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "fdname", &obj->fdname, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_MemoryInfo_members(Visitor *v, MemoryInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_size(v, "base-memory", &obj->base_memory, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "plugged-memory", &obj->has_plugged_memory)) {
        visit_type_size(v, "plugged-memory", &obj->plugged_memory, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_MemoryInfo(Visitor *v, const char *name, MemoryInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(MemoryInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_MemoryInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_MemoryInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_AddfdInfo_members(Visitor *v, AddfdInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "fdset-id", &obj->fdset_id, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "fd", &obj->fd, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_AddfdInfo(Visitor *v, const char *name, AddfdInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(AddfdInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_AddfdInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_AddfdInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_add_fd_arg_members(Visitor *v, q_obj_add_fd_arg *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "fdset-id", &obj->has_fdset_id)) {
        visit_type_int(v, "fdset-id", &obj->fdset_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "opaque", &obj->has_opaque)) {
        visit_type_str(v, "opaque", &obj->opaque, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_remove_fd_arg_members(Visitor *v, q_obj_remove_fd_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "fdset-id", &obj->fdset_id, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "fd", &obj->has_fd)) {
        visit_type_int(v, "fd", &obj->fd, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_FdsetFdInfo_members(Visitor *v, FdsetFdInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "fd", &obj->fd, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "opaque", &obj->has_opaque)) {
        visit_type_str(v, "opaque", &obj->opaque, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_FdsetFdInfo(Visitor *v, const char *name, FdsetFdInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(FdsetFdInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_FdsetFdInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_FdsetFdInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_FdsetFdInfoList(Visitor *v, const char *name, FdsetFdInfoList **obj, Error **errp)
{
    Error *err = NULL;
    FdsetFdInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (FdsetFdInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_FdsetFdInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_FdsetFdInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_FdsetInfo_members(Visitor *v, FdsetInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "fdset-id", &obj->fdset_id, &err);
    if (err) {
        goto out;
    }
    visit_type_FdsetFdInfoList(v, "fds", &obj->fds, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_FdsetInfo(Visitor *v, const char *name, FdsetInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(FdsetInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_FdsetInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_FdsetInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_FdsetInfoList(Visitor *v, const char *name, FdsetInfoList **obj, Error **errp)
{
    Error *err = NULL;
    FdsetInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (FdsetInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_FdsetInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_FdsetInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_AcpiTableOptions_members(Visitor *v, AcpiTableOptions *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "sig", &obj->has_sig)) {
        visit_type_str(v, "sig", &obj->sig, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "rev", &obj->has_rev)) {
        visit_type_uint8(v, "rev", &obj->rev, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "oem_id", &obj->has_oem_id)) {
        visit_type_str(v, "oem_id", &obj->oem_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "oem_table_id", &obj->has_oem_table_id)) {
        visit_type_str(v, "oem_table_id", &obj->oem_table_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "oem_rev", &obj->has_oem_rev)) {
        visit_type_uint32(v, "oem_rev", &obj->oem_rev, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "asl_compiler_id", &obj->has_asl_compiler_id)) {
        visit_type_str(v, "asl_compiler_id", &obj->asl_compiler_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "asl_compiler_rev", &obj->has_asl_compiler_rev)) {
        visit_type_uint32(v, "asl_compiler_rev", &obj->asl_compiler_rev, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "file", &obj->has_file)) {
        visit_type_str(v, "file", &obj->file, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "data", &obj->has_data)) {
        visit_type_str(v, "data", &obj->data, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_AcpiTableOptions(Visitor *v, const char *name, AcpiTableOptions **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(AcpiTableOptions), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_AcpiTableOptions_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_AcpiTableOptions(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CommandLineParameterType(Visitor *v, const char *name, CommandLineParameterType *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &CommandLineParameterType_lookup, errp);
    *obj = value;
}

void visit_type_CommandLineParameterInfo_members(Visitor *v, CommandLineParameterInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "name", &obj->name, &err);
    if (err) {
        goto out;
    }
    visit_type_CommandLineParameterType(v, "type", &obj->type, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "help", &obj->has_help)) {
        visit_type_str(v, "help", &obj->help, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "default", &obj->has_q_default)) {
        visit_type_str(v, "default", &obj->q_default, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_CommandLineParameterInfo(Visitor *v, const char *name, CommandLineParameterInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CommandLineParameterInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CommandLineParameterInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CommandLineParameterInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CommandLineParameterInfoList(Visitor *v, const char *name, CommandLineParameterInfoList **obj, Error **errp)
{
    Error *err = NULL;
    CommandLineParameterInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (CommandLineParameterInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_CommandLineParameterInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CommandLineParameterInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CommandLineOptionInfo_members(Visitor *v, CommandLineOptionInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "option", &obj->option, &err);
    if (err) {
        goto out;
    }
    visit_type_CommandLineParameterInfoList(v, "parameters", &obj->parameters, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CommandLineOptionInfo(Visitor *v, const char *name, CommandLineOptionInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CommandLineOptionInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CommandLineOptionInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CommandLineOptionInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_query_command_line_options_arg_members(Visitor *v, q_obj_query_command_line_options_arg *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "option", &obj->has_option)) {
        visit_type_str(v, "option", &obj->option, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_CommandLineOptionInfoList(Visitor *v, const char *name, CommandLineOptionInfoList **obj, Error **errp)
{
    Error *err = NULL;
    CommandLineOptionInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (CommandLineOptionInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_CommandLineOptionInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CommandLineOptionInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_PCDIMMDeviceInfo_members(Visitor *v, PCDIMMDeviceInfo *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "id", &obj->has_id)) {
        visit_type_str(v, "id", &obj->id, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_int(v, "addr", &obj->addr, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "size", &obj->size, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "slot", &obj->slot, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "node", &obj->node, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "memdev", &obj->memdev, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "hotplugged", &obj->hotplugged, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "hotpluggable", &obj->hotpluggable, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_PCDIMMDeviceInfo(Visitor *v, const char *name, PCDIMMDeviceInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(PCDIMMDeviceInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_PCDIMMDeviceInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_PCDIMMDeviceInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_VirtioPMEMDeviceInfo_members(Visitor *v, VirtioPMEMDeviceInfo *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "id", &obj->has_id)) {
        visit_type_str(v, "id", &obj->id, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_size(v, "memaddr", &obj->memaddr, &err);
    if (err) {
        goto out;
    }
    visit_type_size(v, "size", &obj->size, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "memdev", &obj->memdev, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_VirtioPMEMDeviceInfo(Visitor *v, const char *name, VirtioPMEMDeviceInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(VirtioPMEMDeviceInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_VirtioPMEMDeviceInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_VirtioPMEMDeviceInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_PCDIMMDeviceInfo_wrapper_members(Visitor *v, q_obj_PCDIMMDeviceInfo_wrapper *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_PCDIMMDeviceInfo(v, "data", &obj->data, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_q_obj_VirtioPMEMDeviceInfo_wrapper_members(Visitor *v, q_obj_VirtioPMEMDeviceInfo_wrapper *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_VirtioPMEMDeviceInfo(v, "data", &obj->data, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_MemoryDeviceInfoKind(Visitor *v, const char *name, MemoryDeviceInfoKind *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &MemoryDeviceInfoKind_lookup, errp);
    *obj = value;
}

void visit_type_MemoryDeviceInfo_members(Visitor *v, MemoryDeviceInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_MemoryDeviceInfoKind(v, "type", &obj->type, &err);
    if (err) {
        goto out;
    }
    switch (obj->type) {
    case MEMORY_DEVICE_INFO_KIND_DIMM:
        visit_type_q_obj_PCDIMMDeviceInfo_wrapper_members(v, &obj->u.dimm, &err);
        break;
    case MEMORY_DEVICE_INFO_KIND_NVDIMM:
        visit_type_q_obj_PCDIMMDeviceInfo_wrapper_members(v, &obj->u.nvdimm, &err);
        break;
    case MEMORY_DEVICE_INFO_KIND_VIRTIO_PMEM:
        visit_type_q_obj_VirtioPMEMDeviceInfo_wrapper_members(v, &obj->u.virtio_pmem, &err);
        break;
    default:
        abort();
    }

out:
    error_propagate(errp, err);
}

void visit_type_MemoryDeviceInfo(Visitor *v, const char *name, MemoryDeviceInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(MemoryDeviceInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_MemoryDeviceInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_MemoryDeviceInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_MemoryDeviceInfoList(Visitor *v, const char *name, MemoryDeviceInfoList **obj, Error **errp)
{
    Error *err = NULL;
    MemoryDeviceInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (MemoryDeviceInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_MemoryDeviceInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_MemoryDeviceInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_MEM_UNPLUG_ERROR_arg_members(Visitor *v, q_obj_MEM_UNPLUG_ERROR_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "device", &obj->device, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "msg", &obj->msg, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_ACPISlotType(Visitor *v, const char *name, ACPISlotType *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &ACPISlotType_lookup, errp);
    *obj = value;
}

void visit_type_ACPIOSTInfo_members(Visitor *v, ACPIOSTInfo *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "device", &obj->has_device)) {
        visit_type_str(v, "device", &obj->device, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_str(v, "slot", &obj->slot, &err);
    if (err) {
        goto out;
    }
    visit_type_ACPISlotType(v, "slot-type", &obj->slot_type, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "source", &obj->source, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "status", &obj->status, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_ACPIOSTInfo(Visitor *v, const char *name, ACPIOSTInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(ACPIOSTInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_ACPIOSTInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_ACPIOSTInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_ACPIOSTInfoList(Visitor *v, const char *name, ACPIOSTInfoList **obj, Error **errp)
{
    Error *err = NULL;
    ACPIOSTInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (ACPIOSTInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_ACPIOSTInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_ACPIOSTInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_ACPI_DEVICE_OST_arg_members(Visitor *v, q_obj_ACPI_DEVICE_OST_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_ACPIOSTInfo(v, "info", &obj->info, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_ReplayMode(Visitor *v, const char *name, ReplayMode *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &ReplayMode_lookup, errp);
    *obj = value;
}

void visit_type_q_obj_xen_load_devices_state_arg_members(Visitor *v, q_obj_xen_load_devices_state_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "filename", &obj->filename, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_GuidInfo_members(Visitor *v, GuidInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "guid", &obj->guid, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_GuidInfo(Visitor *v, const char *name, GuidInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(GuidInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_GuidInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_GuidInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

/* Dummy declaration to prevent empty .o file */
char qapi_dummy_qapi_visit_misc_c;
