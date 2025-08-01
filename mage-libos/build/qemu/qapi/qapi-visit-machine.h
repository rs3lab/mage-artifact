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

#ifndef QAPI_VISIT_MACHINE_H
#define QAPI_VISIT_MACHINE_H

#include "qapi/qapi-builtin-visit.h"
#include "qapi-types-machine.h"

void visit_type_SysEmuTarget(Visitor *v, const char *name, SysEmuTarget *obj, Error **errp);
void visit_type_CpuInfoArch(Visitor *v, const char *name, CpuInfoArch *obj, Error **errp);

void visit_type_q_obj_CpuInfo_base_members(Visitor *v, q_obj_CpuInfo_base *obj, Error **errp);

void visit_type_CpuInfo_members(Visitor *v, CpuInfo *obj, Error **errp);
void visit_type_CpuInfo(Visitor *v, const char *name, CpuInfo **obj, Error **errp);

void visit_type_CpuInfoX86_members(Visitor *v, CpuInfoX86 *obj, Error **errp);
void visit_type_CpuInfoX86(Visitor *v, const char *name, CpuInfoX86 **obj, Error **errp);

void visit_type_CpuInfoSPARC_members(Visitor *v, CpuInfoSPARC *obj, Error **errp);
void visit_type_CpuInfoSPARC(Visitor *v, const char *name, CpuInfoSPARC **obj, Error **errp);

void visit_type_CpuInfoPPC_members(Visitor *v, CpuInfoPPC *obj, Error **errp);
void visit_type_CpuInfoPPC(Visitor *v, const char *name, CpuInfoPPC **obj, Error **errp);

void visit_type_CpuInfoMIPS_members(Visitor *v, CpuInfoMIPS *obj, Error **errp);
void visit_type_CpuInfoMIPS(Visitor *v, const char *name, CpuInfoMIPS **obj, Error **errp);

void visit_type_CpuInfoTricore_members(Visitor *v, CpuInfoTricore *obj, Error **errp);
void visit_type_CpuInfoTricore(Visitor *v, const char *name, CpuInfoTricore **obj, Error **errp);

void visit_type_CpuInfoRISCV_members(Visitor *v, CpuInfoRISCV *obj, Error **errp);
void visit_type_CpuInfoRISCV(Visitor *v, const char *name, CpuInfoRISCV **obj, Error **errp);
void visit_type_CpuS390State(Visitor *v, const char *name, CpuS390State *obj, Error **errp);

void visit_type_CpuInfoS390_members(Visitor *v, CpuInfoS390 *obj, Error **errp);
void visit_type_CpuInfoS390(Visitor *v, const char *name, CpuInfoS390 **obj, Error **errp);
void visit_type_CpuInfoList(Visitor *v, const char *name, CpuInfoList **obj, Error **errp);

void visit_type_q_obj_CpuInfoFast_base_members(Visitor *v, q_obj_CpuInfoFast_base *obj, Error **errp);

void visit_type_CpuInfoFast_members(Visitor *v, CpuInfoFast *obj, Error **errp);
void visit_type_CpuInfoFast(Visitor *v, const char *name, CpuInfoFast **obj, Error **errp);
void visit_type_CpuInfoFastList(Visitor *v, const char *name, CpuInfoFastList **obj, Error **errp);

void visit_type_q_obj_cpu_add_arg_members(Visitor *v, q_obj_cpu_add_arg *obj, Error **errp);

void visit_type_MachineInfo_members(Visitor *v, MachineInfo *obj, Error **errp);
void visit_type_MachineInfo(Visitor *v, const char *name, MachineInfo **obj, Error **errp);
void visit_type_MachineInfoList(Visitor *v, const char *name, MachineInfoList **obj, Error **errp);

void visit_type_CurrentMachineParams_members(Visitor *v, CurrentMachineParams *obj, Error **errp);
void visit_type_CurrentMachineParams(Visitor *v, const char *name, CurrentMachineParams **obj, Error **errp);

void visit_type_TargetInfo_members(Visitor *v, TargetInfo *obj, Error **errp);
void visit_type_TargetInfo(Visitor *v, const char *name, TargetInfo **obj, Error **errp);
void visit_type_NumaOptionsType(Visitor *v, const char *name, NumaOptionsType *obj, Error **errp);

void visit_type_q_obj_NumaOptions_base_members(Visitor *v, q_obj_NumaOptions_base *obj, Error **errp);

void visit_type_NumaOptions_members(Visitor *v, NumaOptions *obj, Error **errp);
void visit_type_NumaOptions(Visitor *v, const char *name, NumaOptions **obj, Error **errp);

void visit_type_NumaNodeOptions_members(Visitor *v, NumaNodeOptions *obj, Error **errp);
void visit_type_NumaNodeOptions(Visitor *v, const char *name, NumaNodeOptions **obj, Error **errp);

void visit_type_NumaDistOptions_members(Visitor *v, NumaDistOptions *obj, Error **errp);
void visit_type_NumaDistOptions(Visitor *v, const char *name, NumaDistOptions **obj, Error **errp);
void visit_type_X86CPURegister32(Visitor *v, const char *name, X86CPURegister32 *obj, Error **errp);

void visit_type_X86CPUFeatureWordInfo_members(Visitor *v, X86CPUFeatureWordInfo *obj, Error **errp);
void visit_type_X86CPUFeatureWordInfo(Visitor *v, const char *name, X86CPUFeatureWordInfo **obj, Error **errp);
void visit_type_X86CPUFeatureWordInfoList(Visitor *v, const char *name, X86CPUFeatureWordInfoList **obj, Error **errp);

void visit_type_DummyForceArrays_members(Visitor *v, DummyForceArrays *obj, Error **errp);
void visit_type_DummyForceArrays(Visitor *v, const char *name, DummyForceArrays **obj, Error **errp);

void visit_type_NumaCpuOptions_members(Visitor *v, NumaCpuOptions *obj, Error **errp);
void visit_type_NumaCpuOptions(Visitor *v, const char *name, NumaCpuOptions **obj, Error **errp);
void visit_type_HostMemPolicy(Visitor *v, const char *name, HostMemPolicy *obj, Error **errp);

void visit_type_Memdev_members(Visitor *v, Memdev *obj, Error **errp);
void visit_type_Memdev(Visitor *v, const char *name, Memdev **obj, Error **errp);
void visit_type_MemdevList(Visitor *v, const char *name, MemdevList **obj, Error **errp);

void visit_type_CpuInstanceProperties_members(Visitor *v, CpuInstanceProperties *obj, Error **errp);
void visit_type_CpuInstanceProperties(Visitor *v, const char *name, CpuInstanceProperties **obj, Error **errp);

void visit_type_HotpluggableCPU_members(Visitor *v, HotpluggableCPU *obj, Error **errp);
void visit_type_HotpluggableCPU(Visitor *v, const char *name, HotpluggableCPU **obj, Error **errp);
void visit_type_HotpluggableCPUList(Visitor *v, const char *name, HotpluggableCPUList **obj, Error **errp);

#endif /* QAPI_VISIT_MACHINE_H */
