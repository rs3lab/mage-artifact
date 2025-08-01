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

#ifndef QAPI_COMMANDS_MISC_H
#define QAPI_COMMANDS_MISC_H

#include "qapi-commands-common.h"
#include "qapi-types-misc.h"
#include "qapi/qmp/dispatch.h"

void qmp_qmp_capabilities(bool has_enable, QMPCapabilityList *enable, Error **errp);
void qmp_marshal_qmp_capabilities(QDict *args, QObject **ret, Error **errp);
VersionInfo *qmp_query_version(Error **errp);
void qmp_marshal_query_version(QDict *args, QObject **ret, Error **errp);
CommandInfoList *qmp_query_commands(Error **errp);
void qmp_marshal_query_commands(QDict *args, QObject **ret, Error **errp);
void qmp_add_client(const char *protocol, const char *fdname, bool has_skipauth, bool skipauth, bool has_tls, bool tls, Error **errp);
void qmp_marshal_add_client(QDict *args, QObject **ret, Error **errp);
NameInfo *qmp_query_name(Error **errp);
void qmp_marshal_query_name(QDict *args, QObject **ret, Error **errp);
KvmInfo *qmp_query_kvm(Error **errp);
void qmp_marshal_query_kvm(QDict *args, QObject **ret, Error **errp);
UuidInfo *qmp_query_uuid(Error **errp);
void qmp_marshal_query_uuid(QDict *args, QObject **ret, Error **errp);
EventInfoList *qmp_query_events(Error **errp);
void qmp_marshal_query_events(QDict *args, QObject **ret, Error **errp);
IOThreadInfoList *qmp_query_iothreads(Error **errp);
void qmp_marshal_query_iothreads(QDict *args, QObject **ret, Error **errp);
BalloonInfo *qmp_query_balloon(Error **errp);
void qmp_marshal_query_balloon(QDict *args, QObject **ret, Error **errp);
PciInfoList *qmp_query_pci(Error **errp);
void qmp_marshal_query_pci(QDict *args, QObject **ret, Error **errp);
void qmp_quit(Error **errp);
void qmp_marshal_quit(QDict *args, QObject **ret, Error **errp);
void qmp_stop(Error **errp);
void qmp_marshal_stop(QDict *args, QObject **ret, Error **errp);
void qmp_system_reset(Error **errp);
void qmp_marshal_system_reset(QDict *args, QObject **ret, Error **errp);
void qmp_system_powerdown(Error **errp);
void qmp_marshal_system_powerdown(QDict *args, QObject **ret, Error **errp);
void qmp_memsave(int64_t val, int64_t size, const char *filename, bool has_cpu_index, int64_t cpu_index, Error **errp);
void qmp_marshal_memsave(QDict *args, QObject **ret, Error **errp);
void qmp_pmemsave(int64_t val, int64_t size, const char *filename, Error **errp);
void qmp_marshal_pmemsave(QDict *args, QObject **ret, Error **errp);
void qmp_cont(Error **errp);
void qmp_marshal_cont(QDict *args, QObject **ret, Error **errp);
void qmp_x_exit_preconfig(Error **errp);
void qmp_marshal_x_exit_preconfig(QDict *args, QObject **ret, Error **errp);
void qmp_system_wakeup(Error **errp);
void qmp_marshal_system_wakeup(QDict *args, QObject **ret, Error **errp);
void qmp_inject_nmi(Error **errp);
void qmp_marshal_inject_nmi(QDict *args, QObject **ret, Error **errp);
void qmp_balloon(int64_t value, Error **errp);
void qmp_marshal_balloon(QDict *args, QObject **ret, Error **errp);
char *qmp_human_monitor_command(const char *command_line, bool has_cpu_index, int64_t cpu_index, Error **errp);
void qmp_marshal_human_monitor_command(QDict *args, QObject **ret, Error **errp);
void qmp_change(const char *device, const char *target, bool has_arg, const char *arg, Error **errp);
void qmp_marshal_change(QDict *args, QObject **ret, Error **errp);
void qmp_xen_set_global_dirty_log(bool enable, Error **errp);
void qmp_marshal_xen_set_global_dirty_log(QDict *args, QObject **ret, Error **errp);
void qmp_getfd(const char *fdname, Error **errp);
void qmp_marshal_getfd(QDict *args, QObject **ret, Error **errp);
void qmp_closefd(const char *fdname, Error **errp);
void qmp_marshal_closefd(QDict *args, QObject **ret, Error **errp);
MemoryInfo *qmp_query_memory_size_summary(Error **errp);
void qmp_marshal_query_memory_size_summary(QDict *args, QObject **ret, Error **errp);
AddfdInfo *qmp_add_fd(bool has_fdset_id, int64_t fdset_id, bool has_opaque, const char *opaque, Error **errp);
void qmp_marshal_add_fd(QDict *args, QObject **ret, Error **errp);
void qmp_remove_fd(int64_t fdset_id, bool has_fd, int64_t fd, Error **errp);
void qmp_marshal_remove_fd(QDict *args, QObject **ret, Error **errp);
FdsetInfoList *qmp_query_fdsets(Error **errp);
void qmp_marshal_query_fdsets(QDict *args, QObject **ret, Error **errp);
CommandLineOptionInfoList *qmp_query_command_line_options(bool has_option, const char *option, Error **errp);
void qmp_marshal_query_command_line_options(QDict *args, QObject **ret, Error **errp);
MemoryDeviceInfoList *qmp_query_memory_devices(Error **errp);
void qmp_marshal_query_memory_devices(QDict *args, QObject **ret, Error **errp);
ACPIOSTInfoList *qmp_query_acpi_ospm_status(Error **errp);
void qmp_marshal_query_acpi_ospm_status(QDict *args, QObject **ret, Error **errp);
void qmp_xen_load_devices_state(const char *filename, Error **errp);
void qmp_marshal_xen_load_devices_state(QDict *args, QObject **ret, Error **errp);
GuidInfo *qmp_query_vm_generation_id(Error **errp);
void qmp_marshal_query_vm_generation_id(QDict *args, QObject **ret, Error **errp);

#endif /* QAPI_COMMANDS_MISC_H */
