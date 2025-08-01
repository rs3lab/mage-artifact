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

#ifndef QAPI_TYPES_MISC_H
#define QAPI_TYPES_MISC_H

#include "qapi/qapi-builtin-types.h"
#include "qapi-types-common.h"

typedef struct QMPCapabilityList QMPCapabilityList;

typedef struct q_obj_qmp_capabilities_arg q_obj_qmp_capabilities_arg;

typedef enum QMPCapability {
    QMP_CAPABILITY_OOB,
    QMP_CAPABILITY__MAX,
} QMPCapability;

#define QMPCapability_str(val) \
    qapi_enum_lookup(&QMPCapability_lookup, (val))

extern const QEnumLookup QMPCapability_lookup;

typedef struct VersionTriple VersionTriple;

typedef struct VersionInfo VersionInfo;

typedef struct CommandInfo CommandInfo;

typedef struct CommandInfoList CommandInfoList;

typedef enum LostTickPolicy {
    LOST_TICK_POLICY_DISCARD,
    LOST_TICK_POLICY_DELAY,
    LOST_TICK_POLICY_SLEW,
    LOST_TICK_POLICY__MAX,
} LostTickPolicy;

#define LostTickPolicy_str(val) \
    qapi_enum_lookup(&LostTickPolicy_lookup, (val))

extern const QEnumLookup LostTickPolicy_lookup;

typedef struct q_obj_add_client_arg q_obj_add_client_arg;

typedef struct NameInfo NameInfo;

typedef struct KvmInfo KvmInfo;

typedef struct UuidInfo UuidInfo;

typedef struct EventInfo EventInfo;

typedef struct EventInfoList EventInfoList;

typedef struct IOThreadInfo IOThreadInfo;

typedef struct IOThreadInfoList IOThreadInfoList;

typedef struct BalloonInfo BalloonInfo;

typedef struct q_obj_BALLOON_CHANGE_arg q_obj_BALLOON_CHANGE_arg;

typedef struct PciMemoryRange PciMemoryRange;

typedef struct PciMemoryRegion PciMemoryRegion;

typedef struct PciBusInfo PciBusInfo;

typedef struct PciDeviceInfoList PciDeviceInfoList;

typedef struct PciBridgeInfo PciBridgeInfo;

typedef struct PciDeviceClass PciDeviceClass;

typedef struct PciDeviceId PciDeviceId;

typedef struct PciMemoryRegionList PciMemoryRegionList;

typedef struct PciDeviceInfo PciDeviceInfo;

typedef struct PciInfo PciInfo;

typedef struct PciInfoList PciInfoList;

typedef struct q_obj_memsave_arg q_obj_memsave_arg;

typedef struct q_obj_pmemsave_arg q_obj_pmemsave_arg;

typedef struct q_obj_balloon_arg q_obj_balloon_arg;

typedef struct q_obj_human_monitor_command_arg q_obj_human_monitor_command_arg;

typedef struct q_obj_change_arg q_obj_change_arg;

typedef struct q_obj_xen_set_global_dirty_log_arg q_obj_xen_set_global_dirty_log_arg;

typedef struct q_obj_getfd_arg q_obj_getfd_arg;

typedef struct q_obj_closefd_arg q_obj_closefd_arg;

typedef struct MemoryInfo MemoryInfo;

typedef struct AddfdInfo AddfdInfo;

typedef struct q_obj_add_fd_arg q_obj_add_fd_arg;

typedef struct q_obj_remove_fd_arg q_obj_remove_fd_arg;

typedef struct FdsetFdInfo FdsetFdInfo;

typedef struct FdsetFdInfoList FdsetFdInfoList;

typedef struct FdsetInfo FdsetInfo;

typedef struct FdsetInfoList FdsetInfoList;

typedef struct AcpiTableOptions AcpiTableOptions;

typedef enum CommandLineParameterType {
    COMMAND_LINE_PARAMETER_TYPE_STRING,
    COMMAND_LINE_PARAMETER_TYPE_BOOLEAN,
    COMMAND_LINE_PARAMETER_TYPE_NUMBER,
    COMMAND_LINE_PARAMETER_TYPE_SIZE,
    COMMAND_LINE_PARAMETER_TYPE__MAX,
} CommandLineParameterType;

#define CommandLineParameterType_str(val) \
    qapi_enum_lookup(&CommandLineParameterType_lookup, (val))

extern const QEnumLookup CommandLineParameterType_lookup;

typedef struct CommandLineParameterInfo CommandLineParameterInfo;

typedef struct CommandLineParameterInfoList CommandLineParameterInfoList;

typedef struct CommandLineOptionInfo CommandLineOptionInfo;

typedef struct q_obj_query_command_line_options_arg q_obj_query_command_line_options_arg;

typedef struct CommandLineOptionInfoList CommandLineOptionInfoList;

typedef struct PCDIMMDeviceInfo PCDIMMDeviceInfo;

typedef struct VirtioPMEMDeviceInfo VirtioPMEMDeviceInfo;

typedef struct q_obj_PCDIMMDeviceInfo_wrapper q_obj_PCDIMMDeviceInfo_wrapper;

typedef struct q_obj_VirtioPMEMDeviceInfo_wrapper q_obj_VirtioPMEMDeviceInfo_wrapper;

typedef enum MemoryDeviceInfoKind {
    MEMORY_DEVICE_INFO_KIND_DIMM,
    MEMORY_DEVICE_INFO_KIND_NVDIMM,
    MEMORY_DEVICE_INFO_KIND_VIRTIO_PMEM,
    MEMORY_DEVICE_INFO_KIND__MAX,
} MemoryDeviceInfoKind;

#define MemoryDeviceInfoKind_str(val) \
    qapi_enum_lookup(&MemoryDeviceInfoKind_lookup, (val))

extern const QEnumLookup MemoryDeviceInfoKind_lookup;

typedef struct MemoryDeviceInfo MemoryDeviceInfo;

typedef struct MemoryDeviceInfoList MemoryDeviceInfoList;

typedef struct q_obj_MEM_UNPLUG_ERROR_arg q_obj_MEM_UNPLUG_ERROR_arg;

typedef enum ACPISlotType {
    ACPI_SLOT_TYPE_DIMM,
    ACPI_SLOT_TYPE_CPU,
    ACPI_SLOT_TYPE__MAX,
} ACPISlotType;

#define ACPISlotType_str(val) \
    qapi_enum_lookup(&ACPISlotType_lookup, (val))

extern const QEnumLookup ACPISlotType_lookup;

typedef struct ACPIOSTInfo ACPIOSTInfo;

typedef struct ACPIOSTInfoList ACPIOSTInfoList;

typedef struct q_obj_ACPI_DEVICE_OST_arg q_obj_ACPI_DEVICE_OST_arg;

typedef enum ReplayMode {
    REPLAY_MODE_NONE,
    REPLAY_MODE_RECORD,
    REPLAY_MODE_PLAY,
    REPLAY_MODE__MAX,
} ReplayMode;

#define ReplayMode_str(val) \
    qapi_enum_lookup(&ReplayMode_lookup, (val))

extern const QEnumLookup ReplayMode_lookup;

typedef struct q_obj_xen_load_devices_state_arg q_obj_xen_load_devices_state_arg;

typedef struct GuidInfo GuidInfo;

struct QMPCapabilityList {
    QMPCapabilityList *next;
    QMPCapability value;
};

void qapi_free_QMPCapabilityList(QMPCapabilityList *obj);

struct q_obj_qmp_capabilities_arg {
    bool has_enable;
    QMPCapabilityList *enable;
};

struct VersionTriple {
    int64_t major;
    int64_t minor;
    int64_t micro;
};

void qapi_free_VersionTriple(VersionTriple *obj);

struct VersionInfo {
    VersionTriple *qemu;
    char *package;
};

void qapi_free_VersionInfo(VersionInfo *obj);

struct CommandInfo {
    char *name;
};

void qapi_free_CommandInfo(CommandInfo *obj);

struct CommandInfoList {
    CommandInfoList *next;
    CommandInfo *value;
};

void qapi_free_CommandInfoList(CommandInfoList *obj);

struct q_obj_add_client_arg {
    char *protocol;
    char *fdname;
    bool has_skipauth;
    bool skipauth;
    bool has_tls;
    bool tls;
};

struct NameInfo {
    bool has_name;
    char *name;
};

void qapi_free_NameInfo(NameInfo *obj);

struct KvmInfo {
    bool enabled;
    bool present;
};

void qapi_free_KvmInfo(KvmInfo *obj);

struct UuidInfo {
    char *UUID;
};

void qapi_free_UuidInfo(UuidInfo *obj);

struct EventInfo {
    char *name;
};

void qapi_free_EventInfo(EventInfo *obj);

struct EventInfoList {
    EventInfoList *next;
    EventInfo *value;
};

void qapi_free_EventInfoList(EventInfoList *obj);

struct IOThreadInfo {
    char *id;
    int64_t thread_id;
    int64_t poll_max_ns;
    int64_t poll_grow;
    int64_t poll_shrink;
};

void qapi_free_IOThreadInfo(IOThreadInfo *obj);

struct IOThreadInfoList {
    IOThreadInfoList *next;
    IOThreadInfo *value;
};

void qapi_free_IOThreadInfoList(IOThreadInfoList *obj);

struct BalloonInfo {
    int64_t actual;
};

void qapi_free_BalloonInfo(BalloonInfo *obj);

struct q_obj_BALLOON_CHANGE_arg {
    int64_t actual;
};

struct PciMemoryRange {
    int64_t base;
    int64_t limit;
};

void qapi_free_PciMemoryRange(PciMemoryRange *obj);

struct PciMemoryRegion {
    int64_t bar;
    char *type;
    int64_t address;
    int64_t size;
    bool has_prefetch;
    bool prefetch;
    bool has_mem_type_64;
    bool mem_type_64;
};

void qapi_free_PciMemoryRegion(PciMemoryRegion *obj);

struct PciBusInfo {
    int64_t number;
    int64_t secondary;
    int64_t subordinate;
    PciMemoryRange *io_range;
    PciMemoryRange *memory_range;
    PciMemoryRange *prefetchable_range;
};

void qapi_free_PciBusInfo(PciBusInfo *obj);

struct PciDeviceInfoList {
    PciDeviceInfoList *next;
    PciDeviceInfo *value;
};

void qapi_free_PciDeviceInfoList(PciDeviceInfoList *obj);

struct PciBridgeInfo {
    PciBusInfo *bus;
    bool has_devices;
    PciDeviceInfoList *devices;
};

void qapi_free_PciBridgeInfo(PciBridgeInfo *obj);

struct PciDeviceClass {
    bool has_desc;
    char *desc;
    int64_t q_class;
};

void qapi_free_PciDeviceClass(PciDeviceClass *obj);

struct PciDeviceId {
    int64_t device;
    int64_t vendor;
    bool has_subsystem;
    int64_t subsystem;
    bool has_subsystem_vendor;
    int64_t subsystem_vendor;
};

void qapi_free_PciDeviceId(PciDeviceId *obj);

struct PciMemoryRegionList {
    PciMemoryRegionList *next;
    PciMemoryRegion *value;
};

void qapi_free_PciMemoryRegionList(PciMemoryRegionList *obj);

struct PciDeviceInfo {
    int64_t bus;
    int64_t slot;
    int64_t function;
    PciDeviceClass *class_info;
    PciDeviceId *id;
    bool has_irq;
    int64_t irq;
    char *qdev_id;
    bool has_pci_bridge;
    PciBridgeInfo *pci_bridge;
    PciMemoryRegionList *regions;
};

void qapi_free_PciDeviceInfo(PciDeviceInfo *obj);

struct PciInfo {
    int64_t bus;
    PciDeviceInfoList *devices;
};

void qapi_free_PciInfo(PciInfo *obj);

struct PciInfoList {
    PciInfoList *next;
    PciInfo *value;
};

void qapi_free_PciInfoList(PciInfoList *obj);

struct q_obj_memsave_arg {
    int64_t val;
    int64_t size;
    char *filename;
    bool has_cpu_index;
    int64_t cpu_index;
};

struct q_obj_pmemsave_arg {
    int64_t val;
    int64_t size;
    char *filename;
};

struct q_obj_balloon_arg {
    int64_t value;
};

struct q_obj_human_monitor_command_arg {
    char *command_line;
    bool has_cpu_index;
    int64_t cpu_index;
};

struct q_obj_change_arg {
    char *device;
    char *target;
    bool has_arg;
    char *arg;
};

struct q_obj_xen_set_global_dirty_log_arg {
    bool enable;
};

struct q_obj_getfd_arg {
    char *fdname;
};

struct q_obj_closefd_arg {
    char *fdname;
};

struct MemoryInfo {
    uint64_t base_memory;
    bool has_plugged_memory;
    uint64_t plugged_memory;
};

void qapi_free_MemoryInfo(MemoryInfo *obj);

struct AddfdInfo {
    int64_t fdset_id;
    int64_t fd;
};

void qapi_free_AddfdInfo(AddfdInfo *obj);

struct q_obj_add_fd_arg {
    bool has_fdset_id;
    int64_t fdset_id;
    bool has_opaque;
    char *opaque;
};

struct q_obj_remove_fd_arg {
    int64_t fdset_id;
    bool has_fd;
    int64_t fd;
};

struct FdsetFdInfo {
    int64_t fd;
    bool has_opaque;
    char *opaque;
};

void qapi_free_FdsetFdInfo(FdsetFdInfo *obj);

struct FdsetFdInfoList {
    FdsetFdInfoList *next;
    FdsetFdInfo *value;
};

void qapi_free_FdsetFdInfoList(FdsetFdInfoList *obj);

struct FdsetInfo {
    int64_t fdset_id;
    FdsetFdInfoList *fds;
};

void qapi_free_FdsetInfo(FdsetInfo *obj);

struct FdsetInfoList {
    FdsetInfoList *next;
    FdsetInfo *value;
};

void qapi_free_FdsetInfoList(FdsetInfoList *obj);

struct AcpiTableOptions {
    bool has_sig;
    char *sig;
    bool has_rev;
    uint8_t rev;
    bool has_oem_id;
    char *oem_id;
    bool has_oem_table_id;
    char *oem_table_id;
    bool has_oem_rev;
    uint32_t oem_rev;
    bool has_asl_compiler_id;
    char *asl_compiler_id;
    bool has_asl_compiler_rev;
    uint32_t asl_compiler_rev;
    bool has_file;
    char *file;
    bool has_data;
    char *data;
};

void qapi_free_AcpiTableOptions(AcpiTableOptions *obj);

struct CommandLineParameterInfo {
    char *name;
    CommandLineParameterType type;
    bool has_help;
    char *help;
    bool has_q_default;
    char *q_default;
};

void qapi_free_CommandLineParameterInfo(CommandLineParameterInfo *obj);

struct CommandLineParameterInfoList {
    CommandLineParameterInfoList *next;
    CommandLineParameterInfo *value;
};

void qapi_free_CommandLineParameterInfoList(CommandLineParameterInfoList *obj);

struct CommandLineOptionInfo {
    char *option;
    CommandLineParameterInfoList *parameters;
};

void qapi_free_CommandLineOptionInfo(CommandLineOptionInfo *obj);

struct q_obj_query_command_line_options_arg {
    bool has_option;
    char *option;
};

struct CommandLineOptionInfoList {
    CommandLineOptionInfoList *next;
    CommandLineOptionInfo *value;
};

void qapi_free_CommandLineOptionInfoList(CommandLineOptionInfoList *obj);

struct PCDIMMDeviceInfo {
    bool has_id;
    char *id;
    int64_t addr;
    int64_t size;
    int64_t slot;
    int64_t node;
    char *memdev;
    bool hotplugged;
    bool hotpluggable;
};

void qapi_free_PCDIMMDeviceInfo(PCDIMMDeviceInfo *obj);

struct VirtioPMEMDeviceInfo {
    bool has_id;
    char *id;
    uint64_t memaddr;
    uint64_t size;
    char *memdev;
};

void qapi_free_VirtioPMEMDeviceInfo(VirtioPMEMDeviceInfo *obj);

struct q_obj_PCDIMMDeviceInfo_wrapper {
    PCDIMMDeviceInfo *data;
};

struct q_obj_VirtioPMEMDeviceInfo_wrapper {
    VirtioPMEMDeviceInfo *data;
};

struct MemoryDeviceInfo {
    MemoryDeviceInfoKind type;
    union { /* union tag is @type */
        q_obj_PCDIMMDeviceInfo_wrapper dimm;
        q_obj_PCDIMMDeviceInfo_wrapper nvdimm;
        q_obj_VirtioPMEMDeviceInfo_wrapper virtio_pmem;
    } u;
};

void qapi_free_MemoryDeviceInfo(MemoryDeviceInfo *obj);

struct MemoryDeviceInfoList {
    MemoryDeviceInfoList *next;
    MemoryDeviceInfo *value;
};

void qapi_free_MemoryDeviceInfoList(MemoryDeviceInfoList *obj);

struct q_obj_MEM_UNPLUG_ERROR_arg {
    char *device;
    char *msg;
};

struct ACPIOSTInfo {
    bool has_device;
    char *device;
    char *slot;
    ACPISlotType slot_type;
    int64_t source;
    int64_t status;
};

void qapi_free_ACPIOSTInfo(ACPIOSTInfo *obj);

struct ACPIOSTInfoList {
    ACPIOSTInfoList *next;
    ACPIOSTInfo *value;
};

void qapi_free_ACPIOSTInfoList(ACPIOSTInfoList *obj);

struct q_obj_ACPI_DEVICE_OST_arg {
    ACPIOSTInfo *info;
};

struct q_obj_xen_load_devices_state_arg {
    char *filename;
};

struct GuidInfo {
    char *guid;
};

void qapi_free_GuidInfo(GuidInfo *obj);

#endif /* QAPI_TYPES_MISC_H */
