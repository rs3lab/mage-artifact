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

#ifndef QAPI_VISIT_MISC_H
#define QAPI_VISIT_MISC_H

#include "qapi/qapi-builtin-visit.h"
#include "qapi-types-misc.h"

#include "qapi-visit-common.h"
void visit_type_QMPCapabilityList(Visitor *v, const char *name, QMPCapabilityList **obj, Error **errp);

void visit_type_q_obj_qmp_capabilities_arg_members(Visitor *v, q_obj_qmp_capabilities_arg *obj, Error **errp);
void visit_type_QMPCapability(Visitor *v, const char *name, QMPCapability *obj, Error **errp);

void visit_type_VersionTriple_members(Visitor *v, VersionTriple *obj, Error **errp);
void visit_type_VersionTriple(Visitor *v, const char *name, VersionTriple **obj, Error **errp);

void visit_type_VersionInfo_members(Visitor *v, VersionInfo *obj, Error **errp);
void visit_type_VersionInfo(Visitor *v, const char *name, VersionInfo **obj, Error **errp);

void visit_type_CommandInfo_members(Visitor *v, CommandInfo *obj, Error **errp);
void visit_type_CommandInfo(Visitor *v, const char *name, CommandInfo **obj, Error **errp);
void visit_type_CommandInfoList(Visitor *v, const char *name, CommandInfoList **obj, Error **errp);
void visit_type_LostTickPolicy(Visitor *v, const char *name, LostTickPolicy *obj, Error **errp);

void visit_type_q_obj_add_client_arg_members(Visitor *v, q_obj_add_client_arg *obj, Error **errp);

void visit_type_NameInfo_members(Visitor *v, NameInfo *obj, Error **errp);
void visit_type_NameInfo(Visitor *v, const char *name, NameInfo **obj, Error **errp);

void visit_type_KvmInfo_members(Visitor *v, KvmInfo *obj, Error **errp);
void visit_type_KvmInfo(Visitor *v, const char *name, KvmInfo **obj, Error **errp);

void visit_type_UuidInfo_members(Visitor *v, UuidInfo *obj, Error **errp);
void visit_type_UuidInfo(Visitor *v, const char *name, UuidInfo **obj, Error **errp);

void visit_type_EventInfo_members(Visitor *v, EventInfo *obj, Error **errp);
void visit_type_EventInfo(Visitor *v, const char *name, EventInfo **obj, Error **errp);
void visit_type_EventInfoList(Visitor *v, const char *name, EventInfoList **obj, Error **errp);

void visit_type_IOThreadInfo_members(Visitor *v, IOThreadInfo *obj, Error **errp);
void visit_type_IOThreadInfo(Visitor *v, const char *name, IOThreadInfo **obj, Error **errp);
void visit_type_IOThreadInfoList(Visitor *v, const char *name, IOThreadInfoList **obj, Error **errp);

void visit_type_BalloonInfo_members(Visitor *v, BalloonInfo *obj, Error **errp);
void visit_type_BalloonInfo(Visitor *v, const char *name, BalloonInfo **obj, Error **errp);

void visit_type_q_obj_BALLOON_CHANGE_arg_members(Visitor *v, q_obj_BALLOON_CHANGE_arg *obj, Error **errp);

void visit_type_PciMemoryRange_members(Visitor *v, PciMemoryRange *obj, Error **errp);
void visit_type_PciMemoryRange(Visitor *v, const char *name, PciMemoryRange **obj, Error **errp);

void visit_type_PciMemoryRegion_members(Visitor *v, PciMemoryRegion *obj, Error **errp);
void visit_type_PciMemoryRegion(Visitor *v, const char *name, PciMemoryRegion **obj, Error **errp);

void visit_type_PciBusInfo_members(Visitor *v, PciBusInfo *obj, Error **errp);
void visit_type_PciBusInfo(Visitor *v, const char *name, PciBusInfo **obj, Error **errp);
void visit_type_PciDeviceInfoList(Visitor *v, const char *name, PciDeviceInfoList **obj, Error **errp);

void visit_type_PciBridgeInfo_members(Visitor *v, PciBridgeInfo *obj, Error **errp);
void visit_type_PciBridgeInfo(Visitor *v, const char *name, PciBridgeInfo **obj, Error **errp);

void visit_type_PciDeviceClass_members(Visitor *v, PciDeviceClass *obj, Error **errp);
void visit_type_PciDeviceClass(Visitor *v, const char *name, PciDeviceClass **obj, Error **errp);

void visit_type_PciDeviceId_members(Visitor *v, PciDeviceId *obj, Error **errp);
void visit_type_PciDeviceId(Visitor *v, const char *name, PciDeviceId **obj, Error **errp);
void visit_type_PciMemoryRegionList(Visitor *v, const char *name, PciMemoryRegionList **obj, Error **errp);

void visit_type_PciDeviceInfo_members(Visitor *v, PciDeviceInfo *obj, Error **errp);
void visit_type_PciDeviceInfo(Visitor *v, const char *name, PciDeviceInfo **obj, Error **errp);

void visit_type_PciInfo_members(Visitor *v, PciInfo *obj, Error **errp);
void visit_type_PciInfo(Visitor *v, const char *name, PciInfo **obj, Error **errp);
void visit_type_PciInfoList(Visitor *v, const char *name, PciInfoList **obj, Error **errp);

void visit_type_q_obj_memsave_arg_members(Visitor *v, q_obj_memsave_arg *obj, Error **errp);

void visit_type_q_obj_pmemsave_arg_members(Visitor *v, q_obj_pmemsave_arg *obj, Error **errp);

void visit_type_q_obj_balloon_arg_members(Visitor *v, q_obj_balloon_arg *obj, Error **errp);

void visit_type_q_obj_human_monitor_command_arg_members(Visitor *v, q_obj_human_monitor_command_arg *obj, Error **errp);

void visit_type_q_obj_change_arg_members(Visitor *v, q_obj_change_arg *obj, Error **errp);

void visit_type_q_obj_xen_set_global_dirty_log_arg_members(Visitor *v, q_obj_xen_set_global_dirty_log_arg *obj, Error **errp);

void visit_type_q_obj_getfd_arg_members(Visitor *v, q_obj_getfd_arg *obj, Error **errp);

void visit_type_q_obj_closefd_arg_members(Visitor *v, q_obj_closefd_arg *obj, Error **errp);

void visit_type_MemoryInfo_members(Visitor *v, MemoryInfo *obj, Error **errp);
void visit_type_MemoryInfo(Visitor *v, const char *name, MemoryInfo **obj, Error **errp);

void visit_type_AddfdInfo_members(Visitor *v, AddfdInfo *obj, Error **errp);
void visit_type_AddfdInfo(Visitor *v, const char *name, AddfdInfo **obj, Error **errp);

void visit_type_q_obj_add_fd_arg_members(Visitor *v, q_obj_add_fd_arg *obj, Error **errp);

void visit_type_q_obj_remove_fd_arg_members(Visitor *v, q_obj_remove_fd_arg *obj, Error **errp);

void visit_type_FdsetFdInfo_members(Visitor *v, FdsetFdInfo *obj, Error **errp);
void visit_type_FdsetFdInfo(Visitor *v, const char *name, FdsetFdInfo **obj, Error **errp);
void visit_type_FdsetFdInfoList(Visitor *v, const char *name, FdsetFdInfoList **obj, Error **errp);

void visit_type_FdsetInfo_members(Visitor *v, FdsetInfo *obj, Error **errp);
void visit_type_FdsetInfo(Visitor *v, const char *name, FdsetInfo **obj, Error **errp);
void visit_type_FdsetInfoList(Visitor *v, const char *name, FdsetInfoList **obj, Error **errp);

void visit_type_AcpiTableOptions_members(Visitor *v, AcpiTableOptions *obj, Error **errp);
void visit_type_AcpiTableOptions(Visitor *v, const char *name, AcpiTableOptions **obj, Error **errp);
void visit_type_CommandLineParameterType(Visitor *v, const char *name, CommandLineParameterType *obj, Error **errp);

void visit_type_CommandLineParameterInfo_members(Visitor *v, CommandLineParameterInfo *obj, Error **errp);
void visit_type_CommandLineParameterInfo(Visitor *v, const char *name, CommandLineParameterInfo **obj, Error **errp);
void visit_type_CommandLineParameterInfoList(Visitor *v, const char *name, CommandLineParameterInfoList **obj, Error **errp);

void visit_type_CommandLineOptionInfo_members(Visitor *v, CommandLineOptionInfo *obj, Error **errp);
void visit_type_CommandLineOptionInfo(Visitor *v, const char *name, CommandLineOptionInfo **obj, Error **errp);

void visit_type_q_obj_query_command_line_options_arg_members(Visitor *v, q_obj_query_command_line_options_arg *obj, Error **errp);
void visit_type_CommandLineOptionInfoList(Visitor *v, const char *name, CommandLineOptionInfoList **obj, Error **errp);

void visit_type_PCDIMMDeviceInfo_members(Visitor *v, PCDIMMDeviceInfo *obj, Error **errp);
void visit_type_PCDIMMDeviceInfo(Visitor *v, const char *name, PCDIMMDeviceInfo **obj, Error **errp);

void visit_type_VirtioPMEMDeviceInfo_members(Visitor *v, VirtioPMEMDeviceInfo *obj, Error **errp);
void visit_type_VirtioPMEMDeviceInfo(Visitor *v, const char *name, VirtioPMEMDeviceInfo **obj, Error **errp);

void visit_type_q_obj_PCDIMMDeviceInfo_wrapper_members(Visitor *v, q_obj_PCDIMMDeviceInfo_wrapper *obj, Error **errp);

void visit_type_q_obj_VirtioPMEMDeviceInfo_wrapper_members(Visitor *v, q_obj_VirtioPMEMDeviceInfo_wrapper *obj, Error **errp);
void visit_type_MemoryDeviceInfoKind(Visitor *v, const char *name, MemoryDeviceInfoKind *obj, Error **errp);

void visit_type_MemoryDeviceInfo_members(Visitor *v, MemoryDeviceInfo *obj, Error **errp);
void visit_type_MemoryDeviceInfo(Visitor *v, const char *name, MemoryDeviceInfo **obj, Error **errp);
void visit_type_MemoryDeviceInfoList(Visitor *v, const char *name, MemoryDeviceInfoList **obj, Error **errp);

void visit_type_q_obj_MEM_UNPLUG_ERROR_arg_members(Visitor *v, q_obj_MEM_UNPLUG_ERROR_arg *obj, Error **errp);
void visit_type_ACPISlotType(Visitor *v, const char *name, ACPISlotType *obj, Error **errp);

void visit_type_ACPIOSTInfo_members(Visitor *v, ACPIOSTInfo *obj, Error **errp);
void visit_type_ACPIOSTInfo(Visitor *v, const char *name, ACPIOSTInfo **obj, Error **errp);
void visit_type_ACPIOSTInfoList(Visitor *v, const char *name, ACPIOSTInfoList **obj, Error **errp);

void visit_type_q_obj_ACPI_DEVICE_OST_arg_members(Visitor *v, q_obj_ACPI_DEVICE_OST_arg *obj, Error **errp);
void visit_type_ReplayMode(Visitor *v, const char *name, ReplayMode *obj, Error **errp);

void visit_type_q_obj_xen_load_devices_state_arg_members(Visitor *v, q_obj_xen_load_devices_state_arg *obj, Error **errp);

void visit_type_GuidInfo_members(Visitor *v, GuidInfo *obj, Error **errp);
void visit_type_GuidInfo(Visitor *v, const char *name, GuidInfo **obj, Error **errp);

#endif /* QAPI_VISIT_MISC_H */
