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

#ifndef QAPI_TYPES_MACHINE_H
#define QAPI_TYPES_MACHINE_H

#include "qapi/qapi-builtin-types.h"

typedef enum SysEmuTarget {
    SYS_EMU_TARGET_AARCH64,
    SYS_EMU_TARGET_ALPHA,
    SYS_EMU_TARGET_ARM,
    SYS_EMU_TARGET_CRIS,
    SYS_EMU_TARGET_HPPA,
    SYS_EMU_TARGET_I386,
    SYS_EMU_TARGET_LM32,
    SYS_EMU_TARGET_M68K,
    SYS_EMU_TARGET_MICROBLAZE,
    SYS_EMU_TARGET_MICROBLAZEEL,
    SYS_EMU_TARGET_MIPS,
    SYS_EMU_TARGET_MIPS64,
    SYS_EMU_TARGET_MIPS64EL,
    SYS_EMU_TARGET_MIPSEL,
    SYS_EMU_TARGET_MOXIE,
    SYS_EMU_TARGET_NIOS2,
    SYS_EMU_TARGET_OR1K,
    SYS_EMU_TARGET_PPC,
    SYS_EMU_TARGET_PPC64,
    SYS_EMU_TARGET_RISCV32,
    SYS_EMU_TARGET_RISCV64,
    SYS_EMU_TARGET_S390X,
    SYS_EMU_TARGET_SH4,
    SYS_EMU_TARGET_SH4EB,
    SYS_EMU_TARGET_SPARC,
    SYS_EMU_TARGET_SPARC64,
    SYS_EMU_TARGET_TRICORE,
    SYS_EMU_TARGET_UNICORE32,
    SYS_EMU_TARGET_X86_64,
    SYS_EMU_TARGET_XTENSA,
    SYS_EMU_TARGET_XTENSAEB,
    SYS_EMU_TARGET__MAX,
} SysEmuTarget;

#define SysEmuTarget_str(val) \
    qapi_enum_lookup(&SysEmuTarget_lookup, (val))

extern const QEnumLookup SysEmuTarget_lookup;

typedef enum CpuInfoArch {
    CPU_INFO_ARCH_X86,
    CPU_INFO_ARCH_SPARC,
    CPU_INFO_ARCH_PPC,
    CPU_INFO_ARCH_MIPS,
    CPU_INFO_ARCH_TRICORE,
    CPU_INFO_ARCH_S390,
    CPU_INFO_ARCH_RISCV,
    CPU_INFO_ARCH_OTHER,
    CPU_INFO_ARCH__MAX,
} CpuInfoArch;

#define CpuInfoArch_str(val) \
    qapi_enum_lookup(&CpuInfoArch_lookup, (val))

extern const QEnumLookup CpuInfoArch_lookup;

typedef struct q_obj_CpuInfo_base q_obj_CpuInfo_base;

typedef struct CpuInfo CpuInfo;

typedef struct CpuInfoX86 CpuInfoX86;

typedef struct CpuInfoSPARC CpuInfoSPARC;

typedef struct CpuInfoPPC CpuInfoPPC;

typedef struct CpuInfoMIPS CpuInfoMIPS;

typedef struct CpuInfoTricore CpuInfoTricore;

typedef struct CpuInfoRISCV CpuInfoRISCV;

typedef enum CpuS390State {
    S390_CPU_STATE_UNINITIALIZED,
    S390_CPU_STATE_STOPPED,
    S390_CPU_STATE_CHECK_STOP,
    S390_CPU_STATE_OPERATING,
    S390_CPU_STATE_LOAD,
    S390_CPU_STATE__MAX,
} CpuS390State;

#define CpuS390State_str(val) \
    qapi_enum_lookup(&CpuS390State_lookup, (val))

extern const QEnumLookup CpuS390State_lookup;

typedef struct CpuInfoS390 CpuInfoS390;

typedef struct CpuInfoList CpuInfoList;

typedef struct q_obj_CpuInfoFast_base q_obj_CpuInfoFast_base;

typedef struct CpuInfoFast CpuInfoFast;

typedef struct CpuInfoFastList CpuInfoFastList;

typedef struct q_obj_cpu_add_arg q_obj_cpu_add_arg;

typedef struct MachineInfo MachineInfo;

typedef struct MachineInfoList MachineInfoList;

typedef struct CurrentMachineParams CurrentMachineParams;

typedef struct TargetInfo TargetInfo;

typedef enum NumaOptionsType {
    NUMA_OPTIONS_TYPE_NODE,
    NUMA_OPTIONS_TYPE_DIST,
    NUMA_OPTIONS_TYPE_CPU,
    NUMA_OPTIONS_TYPE__MAX,
} NumaOptionsType;

#define NumaOptionsType_str(val) \
    qapi_enum_lookup(&NumaOptionsType_lookup, (val))

extern const QEnumLookup NumaOptionsType_lookup;

typedef struct q_obj_NumaOptions_base q_obj_NumaOptions_base;

typedef struct NumaOptions NumaOptions;

typedef struct NumaNodeOptions NumaNodeOptions;

typedef struct NumaDistOptions NumaDistOptions;

typedef enum X86CPURegister32 {
    X86_CPU_REGISTER32_EAX,
    X86_CPU_REGISTER32_EBX,
    X86_CPU_REGISTER32_ECX,
    X86_CPU_REGISTER32_EDX,
    X86_CPU_REGISTER32_ESP,
    X86_CPU_REGISTER32_EBP,
    X86_CPU_REGISTER32_ESI,
    X86_CPU_REGISTER32_EDI,
    X86_CPU_REGISTER32__MAX,
} X86CPURegister32;

#define X86CPURegister32_str(val) \
    qapi_enum_lookup(&X86CPURegister32_lookup, (val))

extern const QEnumLookup X86CPURegister32_lookup;

typedef struct X86CPUFeatureWordInfo X86CPUFeatureWordInfo;

typedef struct X86CPUFeatureWordInfoList X86CPUFeatureWordInfoList;

typedef struct DummyForceArrays DummyForceArrays;

typedef struct NumaCpuOptions NumaCpuOptions;

typedef enum HostMemPolicy {
    HOST_MEM_POLICY_DEFAULT,
    HOST_MEM_POLICY_PREFERRED,
    HOST_MEM_POLICY_BIND,
    HOST_MEM_POLICY_INTERLEAVE,
    HOST_MEM_POLICY__MAX,
} HostMemPolicy;

#define HostMemPolicy_str(val) \
    qapi_enum_lookup(&HostMemPolicy_lookup, (val))

extern const QEnumLookup HostMemPolicy_lookup;

typedef struct Memdev Memdev;

typedef struct MemdevList MemdevList;

typedef struct CpuInstanceProperties CpuInstanceProperties;

typedef struct HotpluggableCPU HotpluggableCPU;

typedef struct HotpluggableCPUList HotpluggableCPUList;

struct q_obj_CpuInfo_base {
    int64_t CPU;
    bool current;
    bool halted;
    char *qom_path;
    int64_t thread_id;
    bool has_props;
    CpuInstanceProperties *props;
    CpuInfoArch arch;
};

struct CpuInfoX86 {
    int64_t pc;
};

struct CpuInfoSPARC {
    int64_t pc;
    int64_t npc;
};

struct CpuInfoPPC {
    int64_t nip;
};

struct CpuInfoMIPS {
    int64_t PC;
};

struct CpuInfoTricore {
    int64_t PC;
};

struct CpuInfoS390 {
    CpuS390State cpu_state;
};

struct CpuInfoRISCV {
    int64_t pc;
};

struct CpuInfo {
    int64_t CPU;
    bool current;
    bool halted;
    char *qom_path;
    int64_t thread_id;
    bool has_props;
    CpuInstanceProperties *props;
    CpuInfoArch arch;
    union { /* union tag is @arch */
        CpuInfoX86 x86;
        CpuInfoSPARC q_sparc;
        CpuInfoPPC ppc;
        CpuInfoMIPS q_mips;
        CpuInfoTricore tricore;
        CpuInfoS390 s390;
        CpuInfoRISCV riscv;
    } u;
};

void qapi_free_CpuInfo(CpuInfo *obj);

void qapi_free_CpuInfoX86(CpuInfoX86 *obj);

void qapi_free_CpuInfoSPARC(CpuInfoSPARC *obj);

void qapi_free_CpuInfoPPC(CpuInfoPPC *obj);

void qapi_free_CpuInfoMIPS(CpuInfoMIPS *obj);

void qapi_free_CpuInfoTricore(CpuInfoTricore *obj);

void qapi_free_CpuInfoRISCV(CpuInfoRISCV *obj);

void qapi_free_CpuInfoS390(CpuInfoS390 *obj);

struct CpuInfoList {
    CpuInfoList *next;
    CpuInfo *value;
};

void qapi_free_CpuInfoList(CpuInfoList *obj);

struct q_obj_CpuInfoFast_base {
    int64_t cpu_index;
    char *qom_path;
    int64_t thread_id;
    bool has_props;
    CpuInstanceProperties *props;
    CpuInfoArch arch;
    SysEmuTarget target;
};

struct CpuInfoFast {
    int64_t cpu_index;
    char *qom_path;
    int64_t thread_id;
    bool has_props;
    CpuInstanceProperties *props;
    CpuInfoArch arch;
    SysEmuTarget target;
    union { /* union tag is @target */
        CpuInfoS390 s390x;
    } u;
};

void qapi_free_CpuInfoFast(CpuInfoFast *obj);

struct CpuInfoFastList {
    CpuInfoFastList *next;
    CpuInfoFast *value;
};

void qapi_free_CpuInfoFastList(CpuInfoFastList *obj);

struct q_obj_cpu_add_arg {
    int64_t id;
};

struct MachineInfo {
    char *name;
    bool has_alias;
    char *alias;
    bool has_is_default;
    bool is_default;
    int64_t cpu_max;
    bool hotpluggable_cpus;
    bool numa_mem_supported;
    bool deprecated;
    bool has_default_cpu_type;
    char *default_cpu_type;
};

void qapi_free_MachineInfo(MachineInfo *obj);

struct MachineInfoList {
    MachineInfoList *next;
    MachineInfo *value;
};

void qapi_free_MachineInfoList(MachineInfoList *obj);

struct CurrentMachineParams {
    bool wakeup_suspend_support;
};

void qapi_free_CurrentMachineParams(CurrentMachineParams *obj);

struct TargetInfo {
    SysEmuTarget arch;
};

void qapi_free_TargetInfo(TargetInfo *obj);

struct q_obj_NumaOptions_base {
    NumaOptionsType type;
};

struct NumaNodeOptions {
    bool has_nodeid;
    uint16_t nodeid;
    bool has_cpus;
    uint16List *cpus;
    bool has_mem;
    uint64_t mem;
    bool has_memdev;
    char *memdev;
};

struct NumaDistOptions {
    uint16_t src;
    uint16_t dst;
    uint8_t val;
};

struct NumaCpuOptions {
    /* Members inherited from CpuInstanceProperties: */
    bool has_node_id;
    int64_t node_id;
    bool has_socket_id;
    int64_t socket_id;
    bool has_die_id;
    int64_t die_id;
    bool has_core_id;
    int64_t core_id;
    bool has_thread_id;
    int64_t thread_id;
    /* Own members: */
};

struct NumaOptions {
    NumaOptionsType type;
    union { /* union tag is @type */
        NumaNodeOptions node;
        NumaDistOptions dist;
        NumaCpuOptions cpu;
    } u;
};

void qapi_free_NumaOptions(NumaOptions *obj);

void qapi_free_NumaNodeOptions(NumaNodeOptions *obj);

void qapi_free_NumaDistOptions(NumaDistOptions *obj);

struct X86CPUFeatureWordInfo {
    int64_t cpuid_input_eax;
    bool has_cpuid_input_ecx;
    int64_t cpuid_input_ecx;
    X86CPURegister32 cpuid_register;
    int64_t features;
};

void qapi_free_X86CPUFeatureWordInfo(X86CPUFeatureWordInfo *obj);

struct X86CPUFeatureWordInfoList {
    X86CPUFeatureWordInfoList *next;
    X86CPUFeatureWordInfo *value;
};

void qapi_free_X86CPUFeatureWordInfoList(X86CPUFeatureWordInfoList *obj);

struct DummyForceArrays {
    X86CPUFeatureWordInfoList *unused;
};

void qapi_free_DummyForceArrays(DummyForceArrays *obj);

static inline CpuInstanceProperties *qapi_NumaCpuOptions_base(const NumaCpuOptions *obj)
{
    return (CpuInstanceProperties *)obj;
}

void qapi_free_NumaCpuOptions(NumaCpuOptions *obj);

struct Memdev {
    bool has_id;
    char *id;
    uint64_t size;
    bool merge;
    bool dump;
    bool prealloc;
    uint16List *host_nodes;
    HostMemPolicy policy;
};

void qapi_free_Memdev(Memdev *obj);

struct MemdevList {
    MemdevList *next;
    Memdev *value;
};

void qapi_free_MemdevList(MemdevList *obj);

struct CpuInstanceProperties {
    bool has_node_id;
    int64_t node_id;
    bool has_socket_id;
    int64_t socket_id;
    bool has_die_id;
    int64_t die_id;
    bool has_core_id;
    int64_t core_id;
    bool has_thread_id;
    int64_t thread_id;
};

void qapi_free_CpuInstanceProperties(CpuInstanceProperties *obj);

struct HotpluggableCPU {
    char *type;
    int64_t vcpus_count;
    CpuInstanceProperties *props;
    bool has_qom_path;
    char *qom_path;
};

void qapi_free_HotpluggableCPU(HotpluggableCPU *obj);

struct HotpluggableCPUList {
    HotpluggableCPUList *next;
    HotpluggableCPU *value;
};

void qapi_free_HotpluggableCPUList(HotpluggableCPUList *obj);

#endif /* QAPI_TYPES_MACHINE_H */
