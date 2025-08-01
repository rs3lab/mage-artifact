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
#include "qapi-visit-machine.h"

void visit_type_SysEmuTarget(Visitor *v, const char *name, SysEmuTarget *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &SysEmuTarget_lookup, errp);
    *obj = value;
}

void visit_type_CpuInfoArch(Visitor *v, const char *name, CpuInfoArch *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &CpuInfoArch_lookup, errp);
    *obj = value;
}

void visit_type_q_obj_CpuInfo_base_members(Visitor *v, q_obj_CpuInfo_base *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "CPU", &obj->CPU, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "current", &obj->current, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "halted", &obj->halted, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "qom_path", &obj->qom_path, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "thread_id", &obj->thread_id, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "props", &obj->has_props)) {
        visit_type_CpuInstanceProperties(v, "props", &obj->props, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_CpuInfoArch(v, "arch", &obj->arch, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfo_members(Visitor *v, CpuInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_q_obj_CpuInfo_base_members(v, (q_obj_CpuInfo_base *)obj, &err);
    if (err) {
        goto out;
    }
    switch (obj->arch) {
    case CPU_INFO_ARCH_X86:
        visit_type_CpuInfoX86_members(v, &obj->u.x86, &err);
        break;
    case CPU_INFO_ARCH_SPARC:
        visit_type_CpuInfoSPARC_members(v, &obj->u.q_sparc, &err);
        break;
    case CPU_INFO_ARCH_PPC:
        visit_type_CpuInfoPPC_members(v, &obj->u.ppc, &err);
        break;
    case CPU_INFO_ARCH_MIPS:
        visit_type_CpuInfoMIPS_members(v, &obj->u.q_mips, &err);
        break;
    case CPU_INFO_ARCH_TRICORE:
        visit_type_CpuInfoTricore_members(v, &obj->u.tricore, &err);
        break;
    case CPU_INFO_ARCH_S390:
        visit_type_CpuInfoS390_members(v, &obj->u.s390, &err);
        break;
    case CPU_INFO_ARCH_RISCV:
        visit_type_CpuInfoRISCV_members(v, &obj->u.riscv, &err);
        break;
    case CPU_INFO_ARCH_OTHER:
        break;
    default:
        abort();
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfo(Visitor *v, const char *name, CpuInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoX86_members(Visitor *v, CpuInfoX86 *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "pc", &obj->pc, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoX86(Visitor *v, const char *name, CpuInfoX86 **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoX86), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoX86_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoX86(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoSPARC_members(Visitor *v, CpuInfoSPARC *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "pc", &obj->pc, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "npc", &obj->npc, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoSPARC(Visitor *v, const char *name, CpuInfoSPARC **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoSPARC), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoSPARC_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoSPARC(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoPPC_members(Visitor *v, CpuInfoPPC *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "nip", &obj->nip, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoPPC(Visitor *v, const char *name, CpuInfoPPC **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoPPC), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoPPC_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoPPC(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoMIPS_members(Visitor *v, CpuInfoMIPS *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "PC", &obj->PC, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoMIPS(Visitor *v, const char *name, CpuInfoMIPS **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoMIPS), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoMIPS_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoMIPS(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoTricore_members(Visitor *v, CpuInfoTricore *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "PC", &obj->PC, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoTricore(Visitor *v, const char *name, CpuInfoTricore **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoTricore), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoTricore_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoTricore(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoRISCV_members(Visitor *v, CpuInfoRISCV *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "pc", &obj->pc, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoRISCV(Visitor *v, const char *name, CpuInfoRISCV **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoRISCV), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoRISCV_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoRISCV(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuS390State(Visitor *v, const char *name, CpuS390State *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &CpuS390State_lookup, errp);
    *obj = value;
}

void visit_type_CpuInfoS390_members(Visitor *v, CpuInfoS390 *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuS390State(v, "cpu-state", &obj->cpu_state, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoS390(Visitor *v, const char *name, CpuInfoS390 **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoS390), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoS390_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoS390(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoList(Visitor *v, const char *name, CpuInfoList **obj, Error **errp)
{
    Error *err = NULL;
    CpuInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (CpuInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_CpuInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_CpuInfoFast_base_members(Visitor *v, q_obj_CpuInfoFast_base *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "cpu-index", &obj->cpu_index, &err);
    if (err) {
        goto out;
    }
    visit_type_str(v, "qom-path", &obj->qom_path, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "thread-id", &obj->thread_id, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "props", &obj->has_props)) {
        visit_type_CpuInstanceProperties(v, "props", &obj->props, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_CpuInfoArch(v, "arch", &obj->arch, &err);
    if (err) {
        goto out;
    }
    visit_type_SysEmuTarget(v, "target", &obj->target, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoFast_members(Visitor *v, CpuInfoFast *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_q_obj_CpuInfoFast_base_members(v, (q_obj_CpuInfoFast_base *)obj, &err);
    if (err) {
        goto out;
    }
    switch (obj->target) {
    case SYS_EMU_TARGET_S390X:
        visit_type_CpuInfoS390_members(v, &obj->u.s390x, &err);
        break;
    case SYS_EMU_TARGET_AARCH64:
        break;
    case SYS_EMU_TARGET_ALPHA:
        break;
    case SYS_EMU_TARGET_ARM:
        break;
    case SYS_EMU_TARGET_CRIS:
        break;
    case SYS_EMU_TARGET_HPPA:
        break;
    case SYS_EMU_TARGET_I386:
        break;
    case SYS_EMU_TARGET_LM32:
        break;
    case SYS_EMU_TARGET_M68K:
        break;
    case SYS_EMU_TARGET_MICROBLAZE:
        break;
    case SYS_EMU_TARGET_MICROBLAZEEL:
        break;
    case SYS_EMU_TARGET_MIPS:
        break;
    case SYS_EMU_TARGET_MIPS64:
        break;
    case SYS_EMU_TARGET_MIPS64EL:
        break;
    case SYS_EMU_TARGET_MIPSEL:
        break;
    case SYS_EMU_TARGET_MOXIE:
        break;
    case SYS_EMU_TARGET_NIOS2:
        break;
    case SYS_EMU_TARGET_OR1K:
        break;
    case SYS_EMU_TARGET_PPC:
        break;
    case SYS_EMU_TARGET_PPC64:
        break;
    case SYS_EMU_TARGET_RISCV32:
        break;
    case SYS_EMU_TARGET_RISCV64:
        break;
    case SYS_EMU_TARGET_SH4:
        break;
    case SYS_EMU_TARGET_SH4EB:
        break;
    case SYS_EMU_TARGET_SPARC:
        break;
    case SYS_EMU_TARGET_SPARC64:
        break;
    case SYS_EMU_TARGET_TRICORE:
        break;
    case SYS_EMU_TARGET_UNICORE32:
        break;
    case SYS_EMU_TARGET_X86_64:
        break;
    case SYS_EMU_TARGET_XTENSA:
        break;
    case SYS_EMU_TARGET_XTENSAEB:
        break;
    default:
        abort();
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoFast(Visitor *v, const char *name, CpuInfoFast **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInfoFast), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInfoFast_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoFast(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInfoFastList(Visitor *v, const char *name, CpuInfoFastList **obj, Error **errp)
{
    Error *err = NULL;
    CpuInfoFastList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (CpuInfoFastList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_CpuInfoFast(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInfoFastList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_q_obj_cpu_add_arg_members(Visitor *v, q_obj_cpu_add_arg *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "id", &obj->id, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_MachineInfo_members(Visitor *v, MachineInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "name", &obj->name, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "alias", &obj->has_alias)) {
        visit_type_str(v, "alias", &obj->alias, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "is-default", &obj->has_is_default)) {
        visit_type_bool(v, "is-default", &obj->is_default, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_int(v, "cpu-max", &obj->cpu_max, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "hotpluggable-cpus", &obj->hotpluggable_cpus, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "numa-mem-supported", &obj->numa_mem_supported, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "deprecated", &obj->deprecated, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "default-cpu-type", &obj->has_default_cpu_type)) {
        visit_type_str(v, "default-cpu-type", &obj->default_cpu_type, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_MachineInfo(Visitor *v, const char *name, MachineInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(MachineInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_MachineInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_MachineInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_MachineInfoList(Visitor *v, const char *name, MachineInfoList **obj, Error **errp)
{
    Error *err = NULL;
    MachineInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (MachineInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_MachineInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_MachineInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CurrentMachineParams_members(Visitor *v, CurrentMachineParams *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_bool(v, "wakeup-suspend-support", &obj->wakeup_suspend_support, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_CurrentMachineParams(Visitor *v, const char *name, CurrentMachineParams **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CurrentMachineParams), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CurrentMachineParams_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CurrentMachineParams(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_TargetInfo_members(Visitor *v, TargetInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_SysEmuTarget(v, "arch", &obj->arch, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_TargetInfo(Visitor *v, const char *name, TargetInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(TargetInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_TargetInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_TargetInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_NumaOptionsType(Visitor *v, const char *name, NumaOptionsType *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &NumaOptionsType_lookup, errp);
    *obj = value;
}

void visit_type_q_obj_NumaOptions_base_members(Visitor *v, q_obj_NumaOptions_base *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_NumaOptionsType(v, "type", &obj->type, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_NumaOptions_members(Visitor *v, NumaOptions *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_q_obj_NumaOptions_base_members(v, (q_obj_NumaOptions_base *)obj, &err);
    if (err) {
        goto out;
    }
    switch (obj->type) {
    case NUMA_OPTIONS_TYPE_NODE:
        visit_type_NumaNodeOptions_members(v, &obj->u.node, &err);
        break;
    case NUMA_OPTIONS_TYPE_DIST:
        visit_type_NumaDistOptions_members(v, &obj->u.dist, &err);
        break;
    case NUMA_OPTIONS_TYPE_CPU:
        visit_type_NumaCpuOptions_members(v, &obj->u.cpu, &err);
        break;
    default:
        abort();
    }

out:
    error_propagate(errp, err);
}

void visit_type_NumaOptions(Visitor *v, const char *name, NumaOptions **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(NumaOptions), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_NumaOptions_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_NumaOptions(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_NumaNodeOptions_members(Visitor *v, NumaNodeOptions *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "nodeid", &obj->has_nodeid)) {
        visit_type_uint16(v, "nodeid", &obj->nodeid, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "cpus", &obj->has_cpus)) {
        visit_type_uint16List(v, "cpus", &obj->cpus, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "mem", &obj->has_mem)) {
        visit_type_size(v, "mem", &obj->mem, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "memdev", &obj->has_memdev)) {
        visit_type_str(v, "memdev", &obj->memdev, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_NumaNodeOptions(Visitor *v, const char *name, NumaNodeOptions **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(NumaNodeOptions), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_NumaNodeOptions_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_NumaNodeOptions(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_NumaDistOptions_members(Visitor *v, NumaDistOptions *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_uint16(v, "src", &obj->src, &err);
    if (err) {
        goto out;
    }
    visit_type_uint16(v, "dst", &obj->dst, &err);
    if (err) {
        goto out;
    }
    visit_type_uint8(v, "val", &obj->val, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_NumaDistOptions(Visitor *v, const char *name, NumaDistOptions **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(NumaDistOptions), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_NumaDistOptions_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_NumaDistOptions(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_X86CPURegister32(Visitor *v, const char *name, X86CPURegister32 *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &X86CPURegister32_lookup, errp);
    *obj = value;
}

void visit_type_X86CPUFeatureWordInfo_members(Visitor *v, X86CPUFeatureWordInfo *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_int(v, "cpuid-input-eax", &obj->cpuid_input_eax, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "cpuid-input-ecx", &obj->has_cpuid_input_ecx)) {
        visit_type_int(v, "cpuid-input-ecx", &obj->cpuid_input_ecx, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_X86CPURegister32(v, "cpuid-register", &obj->cpuid_register, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "features", &obj->features, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_X86CPUFeatureWordInfo(Visitor *v, const char *name, X86CPUFeatureWordInfo **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(X86CPUFeatureWordInfo), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_X86CPUFeatureWordInfo_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_X86CPUFeatureWordInfo(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_X86CPUFeatureWordInfoList(Visitor *v, const char *name, X86CPUFeatureWordInfoList **obj, Error **errp)
{
    Error *err = NULL;
    X86CPUFeatureWordInfoList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (X86CPUFeatureWordInfoList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_X86CPUFeatureWordInfo(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_X86CPUFeatureWordInfoList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_DummyForceArrays_members(Visitor *v, DummyForceArrays *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_X86CPUFeatureWordInfoList(v, "unused", &obj->unused, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_DummyForceArrays(Visitor *v, const char *name, DummyForceArrays **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(DummyForceArrays), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_DummyForceArrays_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_DummyForceArrays(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_NumaCpuOptions_members(Visitor *v, NumaCpuOptions *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_CpuInstanceProperties_members(v, (CpuInstanceProperties *)obj, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_NumaCpuOptions(Visitor *v, const char *name, NumaCpuOptions **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(NumaCpuOptions), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_NumaCpuOptions_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_NumaCpuOptions(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_HostMemPolicy(Visitor *v, const char *name, HostMemPolicy *obj, Error **errp)
{
    int value = *obj;
    visit_type_enum(v, name, &value, &HostMemPolicy_lookup, errp);
    *obj = value;
}

void visit_type_Memdev_members(Visitor *v, Memdev *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "id", &obj->has_id)) {
        visit_type_str(v, "id", &obj->id, &err);
        if (err) {
            goto out;
        }
    }
    visit_type_size(v, "size", &obj->size, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "merge", &obj->merge, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "dump", &obj->dump, &err);
    if (err) {
        goto out;
    }
    visit_type_bool(v, "prealloc", &obj->prealloc, &err);
    if (err) {
        goto out;
    }
    visit_type_uint16List(v, "host-nodes", &obj->host_nodes, &err);
    if (err) {
        goto out;
    }
    visit_type_HostMemPolicy(v, "policy", &obj->policy, &err);
    if (err) {
        goto out;
    }

out:
    error_propagate(errp, err);
}

void visit_type_Memdev(Visitor *v, const char *name, Memdev **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(Memdev), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_Memdev_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_Memdev(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_MemdevList(Visitor *v, const char *name, MemdevList **obj, Error **errp)
{
    Error *err = NULL;
    MemdevList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (MemdevList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_Memdev(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_MemdevList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_CpuInstanceProperties_members(Visitor *v, CpuInstanceProperties *obj, Error **errp)
{
    Error *err = NULL;

    if (visit_optional(v, "node-id", &obj->has_node_id)) {
        visit_type_int(v, "node-id", &obj->node_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "socket-id", &obj->has_socket_id)) {
        visit_type_int(v, "socket-id", &obj->socket_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "die-id", &obj->has_die_id)) {
        visit_type_int(v, "die-id", &obj->die_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "core-id", &obj->has_core_id)) {
        visit_type_int(v, "core-id", &obj->core_id, &err);
        if (err) {
            goto out;
        }
    }
    if (visit_optional(v, "thread-id", &obj->has_thread_id)) {
        visit_type_int(v, "thread-id", &obj->thread_id, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_CpuInstanceProperties(Visitor *v, const char *name, CpuInstanceProperties **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(CpuInstanceProperties), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_CpuInstanceProperties_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_CpuInstanceProperties(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_HotpluggableCPU_members(Visitor *v, HotpluggableCPU *obj, Error **errp)
{
    Error *err = NULL;

    visit_type_str(v, "type", &obj->type, &err);
    if (err) {
        goto out;
    }
    visit_type_int(v, "vcpus-count", &obj->vcpus_count, &err);
    if (err) {
        goto out;
    }
    visit_type_CpuInstanceProperties(v, "props", &obj->props, &err);
    if (err) {
        goto out;
    }
    if (visit_optional(v, "qom-path", &obj->has_qom_path)) {
        visit_type_str(v, "qom-path", &obj->qom_path, &err);
        if (err) {
            goto out;
        }
    }

out:
    error_propagate(errp, err);
}

void visit_type_HotpluggableCPU(Visitor *v, const char *name, HotpluggableCPU **obj, Error **errp)
{
    Error *err = NULL;

    visit_start_struct(v, name, (void **)obj, sizeof(HotpluggableCPU), &err);
    if (err) {
        goto out;
    }
    if (!*obj) {
        goto out_obj;
    }
    visit_type_HotpluggableCPU_members(v, *obj, &err);
    if (err) {
        goto out_obj;
    }
    visit_check_struct(v, &err);
out_obj:
    visit_end_struct(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_HotpluggableCPU(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

void visit_type_HotpluggableCPUList(Visitor *v, const char *name, HotpluggableCPUList **obj, Error **errp)
{
    Error *err = NULL;
    HotpluggableCPUList *tail;
    size_t size = sizeof(**obj);

    visit_start_list(v, name, (GenericList **)obj, size, &err);
    if (err) {
        goto out;
    }

    for (tail = *obj; tail;
         tail = (HotpluggableCPUList *)visit_next_list(v, (GenericList *)tail, size)) {
        visit_type_HotpluggableCPU(v, NULL, &tail->value, &err);
        if (err) {
            break;
        }
    }

    if (!err) {
        visit_check_list(v, &err);
    }
    visit_end_list(v, (void **)obj);
    if (err && visit_is_input(v)) {
        qapi_free_HotpluggableCPUList(*obj);
        *obj = NULL;
    }
out:
    error_propagate(errp, err);
}

/* Dummy declaration to prevent empty .o file */
char qapi_dummy_qapi_visit_machine_c;
