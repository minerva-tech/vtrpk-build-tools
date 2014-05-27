# Change the kernel patch to point to Linux source tree
CC = $(CROSS_COMPILE)gcc -O2 -I$(KERNEL_DIR)/include
STRIP = $(CROSS_COMPILE)strip --strip-unneeded
#INSTALL_DIR=$(FINAL_DEST)/mc

all:	teplovisor_ccdc_prv_rsz_file tvp514x_ccdc_prv_rsz_file

teplovisor_ccdc_prv_rsz_file: cmem.o teplovisor_ccdc_prv_rsz_file.o
	$(CC) -o teplovisor_ccdc_prv_rsz_file $^
	$(STRIP) teplovisor_ccdc_prv_rsz_file

tvp514x_ccdc_prv_rsz_file: cmem.o tvp514x_ccdc_prv_rsz_file.o
	$(CC) -o tvp514x_ccdc_prv_rsz_file $^
	$(STRIP) tvp514x_ccdc_prv_rsz_file

install:
	#mkdir -p 				$(INSTALL_DIR)
	#cp teplovisor_ccdc_prv_rsz_file		$(INSTALL_DIR)

clean:
	rm -f *.o *~
	rm -f teplovisor_ccdc_prv_rsz_file
