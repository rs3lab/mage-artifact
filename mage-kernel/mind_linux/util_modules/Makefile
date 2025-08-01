obj-m += fbs_pprint.o
fbs_pprint-objs := ./profile_printer/profile_printer.o
obj-m += fbs_pclean.o
fbs_pclean-objs := ./profile_printer/profile_cleaner.o
obj-m += fbs_psample.o
fbs_psample-objs := ./profile_printer/profile_sampler.o
obj-m += buggen.o
buggen-objs := ./kern_bug_generator/kern_bug_generator.o
all:
	 make -C ../. M=$(PWD) modules
clean:
	 make -C ../. M=$(PWD) clean
