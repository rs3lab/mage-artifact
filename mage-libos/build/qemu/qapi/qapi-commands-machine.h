/* AUTOMATICALLY GENERATED, DO NOT MODIFY */

/*
 * Schema-defined QAPI/QMP commands
 *
 * Copyright IBM, Corp. 2011
 * Copyright (C) 2014-2018 Red Hat, Inc.
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#ifndef QAPI_COMMANDS_MACHINE_H
#define QAPI_COMMANDS_MACHINE_H

#include "qapi-types-machine.h"
#include "qapi/qmp/dispatch.h"

CpuInfoList *qmp_query_cpus(Error **errp);
void qmp_marshal_query_cpus(QDict *args, QObject **ret, Error **errp);
CpuInfoFastList *qmp_query_cpus_fast(Error **errp);
void qmp_marshal_query_cpus_fast(QDict *args, QObject **ret, Error **errp);
void qmp_cpu_add(int64_t id, Error **errp);
void qmp_marshal_cpu_add(QDict *args, QObject **ret, Error **errp);
MachineInfoList *qmp_query_machines(Error **errp);
void qmp_marshal_query_machines(QDict *args, QObject **ret, Error **errp);
CurrentMachineParams *qmp_query_current_machine(Error **errp);
void qmp_marshal_query_current_machine(QDict *args, QObject **ret, Error **errp);
TargetInfo *qmp_query_target(Error **errp);
void qmp_marshal_query_target(QDict *args, QObject **ret, Error **errp);
MemdevList *qmp_query_memdev(Error **errp);
void qmp_marshal_query_memdev(QDict *args, QObject **ret, Error **errp);
HotpluggableCPUList *qmp_query_hotpluggable_cpus(Error **errp);
void qmp_marshal_query_hotpluggable_cpus(QDict *args, QObject **ret, Error **errp);
void qmp_set_numa_node(NumaOptions *arg, Error **errp);
void qmp_marshal_set_numa_node(QDict *args, QObject **ret, Error **errp);

#endif /* QAPI_COMMANDS_MACHINE_H */
