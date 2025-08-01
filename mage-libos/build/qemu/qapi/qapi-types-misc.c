/* AUTOMATICALLY GENERATED, DO NOT MODIFY */

/*
 * Schema-defined QAPI types
 *
 * Copyright IBM, Corp. 2011
 * Copyright (c) 2013-2018 Red Hat Inc.
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/dealloc-visitor.h"
#include "qapi-types-misc.h"
#include "qapi-visit-misc.h"

void qapi_free_QMPCapabilityList(QMPCapabilityList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_QMPCapabilityList(v, NULL, &obj, NULL);
    visit_free(v);
}

const QEnumLookup QMPCapability_lookup = {
    .array = (const char *const[]) {
        [QMP_CAPABILITY_OOB] = "oob",
    },
    .size = QMP_CAPABILITY__MAX
};

void qapi_free_VersionTriple(VersionTriple *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_VersionTriple(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_VersionInfo(VersionInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_VersionInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_CommandInfo(CommandInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_CommandInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_CommandInfoList(CommandInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_CommandInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

const QEnumLookup LostTickPolicy_lookup = {
    .array = (const char *const[]) {
        [LOST_TICK_POLICY_DISCARD] = "discard",
        [LOST_TICK_POLICY_DELAY] = "delay",
        [LOST_TICK_POLICY_SLEW] = "slew",
    },
    .size = LOST_TICK_POLICY__MAX
};

void qapi_free_NameInfo(NameInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_NameInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_KvmInfo(KvmInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_KvmInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_UuidInfo(UuidInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_UuidInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_EventInfo(EventInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_EventInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_EventInfoList(EventInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_EventInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_IOThreadInfo(IOThreadInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_IOThreadInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_IOThreadInfoList(IOThreadInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_IOThreadInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_BalloonInfo(BalloonInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_BalloonInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciMemoryRange(PciMemoryRange *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciMemoryRange(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciMemoryRegion(PciMemoryRegion *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciMemoryRegion(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciBusInfo(PciBusInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciBusInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciDeviceInfoList(PciDeviceInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciDeviceInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciBridgeInfo(PciBridgeInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciBridgeInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciDeviceClass(PciDeviceClass *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciDeviceClass(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciDeviceId(PciDeviceId *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciDeviceId(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciMemoryRegionList(PciMemoryRegionList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciMemoryRegionList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciDeviceInfo(PciDeviceInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciDeviceInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciInfo(PciInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PciInfoList(PciInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PciInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_MemoryInfo(MemoryInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_MemoryInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_AddfdInfo(AddfdInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_AddfdInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_FdsetFdInfo(FdsetFdInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_FdsetFdInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_FdsetFdInfoList(FdsetFdInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_FdsetFdInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_FdsetInfo(FdsetInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_FdsetInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_FdsetInfoList(FdsetInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_FdsetInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_AcpiTableOptions(AcpiTableOptions *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_AcpiTableOptions(v, NULL, &obj, NULL);
    visit_free(v);
}

const QEnumLookup CommandLineParameterType_lookup = {
    .array = (const char *const[]) {
        [COMMAND_LINE_PARAMETER_TYPE_STRING] = "string",
        [COMMAND_LINE_PARAMETER_TYPE_BOOLEAN] = "boolean",
        [COMMAND_LINE_PARAMETER_TYPE_NUMBER] = "number",
        [COMMAND_LINE_PARAMETER_TYPE_SIZE] = "size",
    },
    .size = COMMAND_LINE_PARAMETER_TYPE__MAX
};

void qapi_free_CommandLineParameterInfo(CommandLineParameterInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_CommandLineParameterInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_CommandLineParameterInfoList(CommandLineParameterInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_CommandLineParameterInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_CommandLineOptionInfo(CommandLineOptionInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_CommandLineOptionInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_CommandLineOptionInfoList(CommandLineOptionInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_CommandLineOptionInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_PCDIMMDeviceInfo(PCDIMMDeviceInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_PCDIMMDeviceInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_VirtioPMEMDeviceInfo(VirtioPMEMDeviceInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_VirtioPMEMDeviceInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

const QEnumLookup MemoryDeviceInfoKind_lookup = {
    .array = (const char *const[]) {
        [MEMORY_DEVICE_INFO_KIND_DIMM] = "dimm",
        [MEMORY_DEVICE_INFO_KIND_NVDIMM] = "nvdimm",
        [MEMORY_DEVICE_INFO_KIND_VIRTIO_PMEM] = "virtio-pmem",
    },
    .size = MEMORY_DEVICE_INFO_KIND__MAX
};

void qapi_free_MemoryDeviceInfo(MemoryDeviceInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_MemoryDeviceInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_MemoryDeviceInfoList(MemoryDeviceInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_MemoryDeviceInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

const QEnumLookup ACPISlotType_lookup = {
    .array = (const char *const[]) {
        [ACPI_SLOT_TYPE_DIMM] = "DIMM",
        [ACPI_SLOT_TYPE_CPU] = "CPU",
    },
    .size = ACPI_SLOT_TYPE__MAX
};

void qapi_free_ACPIOSTInfo(ACPIOSTInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_ACPIOSTInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

void qapi_free_ACPIOSTInfoList(ACPIOSTInfoList *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_ACPIOSTInfoList(v, NULL, &obj, NULL);
    visit_free(v);
}

const QEnumLookup ReplayMode_lookup = {
    .array = (const char *const[]) {
        [REPLAY_MODE_NONE] = "none",
        [REPLAY_MODE_RECORD] = "record",
        [REPLAY_MODE_PLAY] = "play",
    },
    .size = REPLAY_MODE__MAX
};

void qapi_free_GuidInfo(GuidInfo *obj)
{
    Visitor *v;

    if (!obj) {
        return;
    }

    v = qapi_dealloc_visitor_new();
    visit_type_GuidInfo(v, NULL, &obj, NULL);
    visit_free(v);
}

/* Dummy declaration to prevent empty .o file */
char qapi_dummy_qapi_types_misc_c;
