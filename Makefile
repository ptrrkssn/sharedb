# Makefile

DEST=/usr/local/sbin
SRCDIR=/usr/src
OBJDIR=/usr/obj/$(SRCDIR)/

LIBZFSSRC=$(SRCDIR)/sys/contrib/openzfs/lib/libshare/os/freebsd/nfs.c
LIBZFSBIN=$(OBJDIR)/amd64.amd64/cddl/lib/libzfs/libzfs.so.4
LIBZFSDIR=/lib

MOUNTDSRC=$(SRCDIR)/usr.sbin/mountd/mountd.c
MOUNTDBIN=$(OBJDIR)/amd64.amd64/usr.sbin/mountd/mountd
MOUNTDDIR=/usr/sbin

SHAREOBJS=share.o

CFLAGS=-O -Wall

all: share buildworld


share: $(SHAREOBJS)
	cc -o share $(OBJS)


clean:
	rm -f *~ *.o share \#* core


build: patch buildworld


patch: patch-zfs patch-mountd

patch-zfs: $(LIBZFSSRC).orig
	patch -sNd $(SRCDIR) <zfs-sharedb.patch

patch-mountd: $(MOUNTDSRC).orig
	patch -sNd $(SRCDIR) <mountd-sharedb.patch



buildworld: $(LIBZFSBIN) $(MOUNTDBIN)
	cd $(SRCDIR) && make buildworld

$(LIBZFSBIN): $(LIBZFSSRC)
$(MOUNTDBIN): $(MOUNTDSRC)


install: install-libzfs install-mountd

install-libzfs: $(LIBZFSBIN)
	install -b $(LIBZFSBIN) $(LIBZFSDIR)

install-mountd: $(MOUNTDBIN)
	install -b $(MOUNTDBIN) $(MOUNTDDIR)
