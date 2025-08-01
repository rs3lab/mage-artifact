/*
 * Main file for CPU pinning logic
 */
#include <disagg/print_disagg.h>
#include <disagg/network_fit_disagg.h>
#include <disagg/cnthread_disagg.h>

#define NUM_ASSIGNABLE_CORES (NUM_ASSIGNABLE_CPUS / 2)

#define FIRST_FH_CORE ((DISAGG_FIRST_ASSIGNABLE_FH_CPU - DISAGG_FIRST_ASSIGNABLE_CPU) / 2)
#define LAST_FH_CORE ((DISAGG_LAST_ASSIGNABLE_FH_CPU - DISAGG_FIRST_ASSIGNABLE_CPU) / 2)
#define FIRST_CN_CORE ((DISAGG_FIRST_ASSIGNABLE_CN_CPU - DISAGG_FIRST_ASSIGNABLE_CPU) / 2)
#define LAST_CN_CORE ((DISAGG_LAST_ASSIGNABLE_CN_CPU - DISAGG_FIRST_ASSIGNABLE_CPU) / 2)

enum cpu_type {
    DISAGG_UNASSIGNED = 0,
    DISAGG_CNTHREAD,
    DISAGG_FHTHREAD,
};

struct cpu_slot {
    enum cpu_type type;
    bool occupied;
} __packed;

struct core {
    struct cpu_slot slots[2];
} __packed;

static struct core *cores;

void disagg_print_core_assignments(void)
{
    int i, j;
    const char cpu_type_chars[] = {
        [DISAGG_UNASSIGNED] = '0',
        [DISAGG_CNTHREAD] = 'C',
        [DISAGG_FHTHREAD] = 'F',
    };

    pr_info("Core Layout:\n");
    pr_info("(First CPU %d, Last CPU %d)\n",
            DISAGG_FIRST_ASSIGNABLE_CPU, DISAGG_LAST_ASSIGNABLE_CPU);
    pr_info("(First FH CPU %d, Last FH CPU %d)\n",
            DISAGG_FIRST_ASSIGNABLE_FH_CPU, DISAGG_LAST_ASSIGNABLE_FH_CPU);
    pr_info("(First CN CPU %d, Last CN CPU %d)\n",
            DISAGG_FIRST_ASSIGNABLE_CN_CPU, DISAGG_LAST_ASSIGNABLE_CN_CPU);
    pr_info("Core   Slot   Type   Occupied\n");
    pr_info("-----------------------------\n");

    for (i = 0; i < NUM_ASSIGNABLE_CORES; i++) {
        for (j = 0; j < 2; j++) {
            struct cpu_slot slot = cores[i].slots[j];
            char cpu_type_char = (slot.type < ARRAY_SIZE(cpu_type_chars))
                    ? cpu_type_chars[slot.type] : '?';

            pr_info("%4d   %4d   %c      %s\n",
                    i, j, cpu_type_char, slot.occupied ? "Yes" : "No");
        }
    }
    pr_info("----------------------------------\n");
}

static void __pin_fhthread(struct task_struct *t, int cpu) {
    struct cpumask *mask = kmalloc(sizeof(*mask), GFP_KERNEL);
    BUG_ON(!mask);
    cpumask_clear(mask);
    cpumask_set_cpu(cpu, mask);
    set_cpus_allowed_ptr(t, mask);
    kfree(mask);
}

static void __pin_cnthread(struct task_struct *t, int cpu) {
    int numa_node = cpu_to_node(cpu);
    WARN_ONCE(numa_node == 1, "cnthreads were pinned to remote NUMA node!");
    kthread_bind(t, cpu);
}

int disagg_pin_fhthread_to_core(struct task_struct *t)
{
    int i;

    // Search for a compatible core that isn't in use yet.
    for (i = 0; i < NUM_ASSIGNABLE_CORES; i++) {
        struct cpu_slot *first_slot = &cores[i].slots[0];
        struct cpu_slot *second_slot = &cores[i].slots[1];

        if (first_slot->type == DISAGG_FHTHREAD && !first_slot->occupied) {
            int cpu = DISAGG_FIRST_ASSIGNABLE_CPU + (2 * i);
            first_slot->occupied = true;
            __pin_fhthread(t, cpu);
            return cpu;
        }

        if (second_slot->type == DISAGG_FHTHREAD && !second_slot->occupied) {
            int cpu = DISAGG_FIRST_ASSIGNABLE_CPU + (2 * i) + 1;
            second_slot->occupied = true;
            __pin_fhthread(t, cpu);
            return cpu;
        }
    }

    disagg_print_core_assignments();
    BUG(); // We ran out of CPUs.
}

int disagg_pin_cnthread_to_core(struct task_struct *t)
{
    int i;

    // Search for a compatible core that isn't in use yet.
    for (i = 0; i < NUM_ASSIGNABLE_CORES; i++) {
        struct cpu_slot *first_slot = &cores[i].slots[0];

        if (first_slot->type == DISAGG_CNTHREAD && !first_slot->occupied) {
            int cpu = DISAGG_FIRST_ASSIGNABLE_CPU + (2 * i);
            first_slot->occupied = true;
            __pin_cnthread(t, cpu);
            return cpu;
        }
    }

    // All compatible cores have at least one hyperthread filled. 
    // We try to find a hyperthreaded slot and colocate with a cnthread.
    for (i = 0; i < NUM_ASSIGNABLE_CORES; i++) {
        struct cpu_slot *second_slot = &cores[i].slots[1];

        if (second_slot->type == DISAGG_CNTHREAD && !second_slot->occupied)
        {
            int cpu = DISAGG_FIRST_ASSIGNABLE_CPU + (2 * i) + 1;
            second_slot->occupied = true;
            __pin_cnthread(t, cpu);
            return cpu;
        }
    }

    // Alas, we can't colocate with a cnthread on a core.
    // We'll have to settle and colocate with a fhthread.
    for (i = 0; i < NUM_ASSIGNABLE_CORES; i++) {
        struct cpu_slot *second_slot = &cores[i].slots[1];

        if (second_slot->type == DISAGG_CNTHREAD && !second_slot->occupied) {
            int cpu = DISAGG_FIRST_ASSIGNABLE_CPU + (2 * i) + 1;
            second_slot->occupied = true;
            __pin_cnthread(t, cpu);
            return cpu;
        }
    }

    disagg_print_core_assignments();
    BUG(); // We ran out of CPUs.
}

// Must call _after_ cnthreads are reserved.
static void __init_reserve_fhthread_core(void)
{
    int i;

    // Try to place app threads with other fhthreads.
    for (i = FIRST_FH_CORE; i <= LAST_FH_CORE; i++) {
        struct core *core = &cores[i];
        if (core->slots[0].type == DISAGG_UNASSIGNED) {
            core->slots[0].type = DISAGG_FHTHREAD;
            return;
        }
        if (core->slots[0].type == DISAGG_FHTHREAD
                && core->slots[1].type == DISAGG_UNASSIGNED) {
            core->slots[1].type = DISAGG_FHTHREAD;
            return;
        }
    }

    // Only if there are no other options, colocate with a cnthread.
    for (i = FIRST_FH_CORE; i <= LAST_FH_CORE; i++) {
        struct core *core = &cores[i];
        if (core->slots[1].type == DISAGG_UNASSIGNED) {
            core->slots[1].type = DISAGG_FHTHREAD;
            return;
        }
    }

    WARN(true, "no more spots when assigning fhthreads!");
    disagg_print_core_assignments();
}

// Must call before fhthread init.
static void __init_reserve_cnthread_core(void)
{
    int i;

    for (i = FIRST_CN_CORE; i <= LAST_CN_CORE; i++) {
        struct core *core = &cores[i];

        if (core->slots[0].type == DISAGG_UNASSIGNED) {
            core->slots[0].type = DISAGG_CNTHREAD;
            return;
        }

        if (core->slots[1].type == DISAGG_UNASSIGNED) {
            core->slots[1].type = DISAGG_CNTHREAD;
            return;
        }
    }

    WARN(true, "no more spots when assigning cnthreads!");
    disagg_print_core_assignments();
}

// TODO: proper cleanup and _thread exit hooks_. Right now, exiting threads will never
//       discard their reserved CPU.
//
// Argument: the max number of FH threads the application expects to use during its run.
void init_disagg_core_pinning(int max_threads_in_use)
{
    int i;

    // Parameter checksums.
    BUILD_BUG_ON(DISAGG_NUM_CORES % 2 != 0);
    BUILD_BUG_ON(NUM_ASSIGNABLE_CPUS % 2 != 0);
    BUILD_BUG_ON(DISAGG_FIRST_ASSIGNABLE_CPU % 2 != 0);
    BUILD_BUG_ON(DISAGG_LAST_ASSIGNABLE_CPU % 2 != 1);
    BUILD_BUG_ON(DISAGG_FIRST_ASSIGNABLE_FH_CPU % 2 != 0);
    BUILD_BUG_ON(DISAGG_LAST_ASSIGNABLE_FH_CPU % 2 != 1);
    BUILD_BUG_ON(DISAGG_FIRST_ASSIGNABLE_CN_CPU % 2 != 0);
    BUILD_BUG_ON(DISAGG_LAST_ASSIGNABLE_CN_CPU % 2 != 1);

    // Logic checksums.
    BUILD_BUG_ON(FIRST_FH_CORE < 0 || FIRST_FH_CORE >= NUM_ASSIGNABLE_CORES);
    BUILD_BUG_ON(LAST_FH_CORE < 0 || LAST_FH_CORE >= NUM_ASSIGNABLE_CORES);
    BUILD_BUG_ON(FIRST_CN_CORE < 0 || FIRST_CN_CORE >= NUM_ASSIGNABLE_CORES);
    BUILD_BUG_ON(LAST_CN_CORE < 0 || LAST_CN_CORE >= NUM_ASSIGNABLE_CORES);
    BUILD_BUG_ON(FIRST_CN_CORE > LAST_CN_CORE || FIRST_FH_CORE > LAST_FH_CORE);

    WARN_ON(max_threads_in_use > NUM_ASSIGNABLE_CPUS);

    cores = kzalloc(sizeof(*cores) * NUM_ASSIGNABLE_CORES, GFP_KERNEL);

    for (i = 0; i < NUM_CNTHREADS; i++)
         __init_reserve_cnthread_core();
    for (i = 0; i < max_threads_in_use; i++)
         __init_reserve_fhthread_core();
}
