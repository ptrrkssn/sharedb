# Makefile

NCPU=8

PREFIX=/usr/local

SRCDIR=/usr/src
OBJDIR=/usr/obj/$(SRCDIR)

SHAREOBJS=share.o
SHAREBINDIR=$(PREFIX)/sbin
SHAREMANDIR=$(PREFIX)/man/man1

LIBZFSSRCDIR=sys/contrib/openzfs/lib/libshare/os/freebsd
LIBZFSOBJDIR=amd64.amd64/cddl/lib/libzfs
LIBZFSLIBDIR=/lib

MOUNTDSRCDIR=usr.sbin/mountd
MOUNTDOBJDIR=amd64.amd64/usr.sbin/mountd
MOUNTDBINDIR=/usr/sbin

CFLAGS=-O -Wall

NOCLEAN= # -DNO_CLEAN


build: share mountd libzfs.so.4

all: apply-patches build-all


apply-patches: patch-mountd patch-libzfs # patch-rcdzfs

patches: mountd-sharedb.patch zfs-sharedb.patch

mountd-sharedb.patch: $(SRCDIR)/$(MOUNTDSRCDIR)/mountd.c
	(cd $(SRCDIR) && git diff $(MOUNTDSRCDIR)/mountd.c) >mountd-sharedb.patch

zfs-sharedb.patch: $(SRCDIR)/$(LIBZFSSRCDIR)/nfs.c
	(cd $(SRCDIR) && git diff $(LIBZFSSRCDIR)/nfs.c) >zfs-sharedb.patch


patch-mountd:
	(cd $(SRCDIR)/$(MOUNTDSRCDIR) && git checkout mountd.c) && \
	patch -sNd $(SRCDIR) <"Patches/mountd-`git --git-dir=$(SRCDIR)/.git branch --show-current|tr '/' '-'`.patch"

patch-libzfs:
	(cd $(SRCDIR)/$(LIBZFSSRCDIR) && git checkout nfs.c) && \
	patch -sNd $(SRCDIR) <"Patches/zfs-`git --git-dir=$(SRCDIR)/.git branch --show-current|tr '/' '-'`.patch"

patch-rcdzfs:
	patch -sNd /etc/rc.d <"Patches/etc-rc.d-zfs.patch"


share: $(SHAREOBJS)
	cc $(CFLAGS) -o share $(SHAREOBJS)

share.o: share.c


mountd: $(OBJDIR)/$(MOUNTDOBJDIR)/mountd
	cp $(OBJDIR)/$(MOUNTDOBJDIR)/mountd .

$(OBJDIR)/$(MOUNTDOBJDIR)/mountd: $(SRCDIR)/$(MOUNTDSRCDIR)/mountd.c
	cd $(SRCDIR)/$(MOUNTDSRCDIR) && make


libzfs.so.4: $(OBJDIR)/$(LIBZFSOBJDIR)/libzfs.so.4
	cp $(OBJDIR)/$(LIBZFSOBJDIR)/libzfs.so.4 .

$(OBJDIR)/$(LIBZFSOBJDIR)/libzfs.so.4: $(SRCDIR)/$(LIBZFSSRCDIR)/nfs.c
	cd $(SRCDIR) && make $(NOCLEAN) -j$(NCPU) buildworld


install-libzfs: $(LIBZFSLIBDIR)/libzfs.so.4

$(LIBZFSLIBDIR)/libzfs.so.4: $(OBJDIR)/$(LIBZFSOBJDIR)/libzfs.so.4
	install -b $(OBJDIR)/$(LIBZFSOBJDIR)/libzfs.so.4 $(LIBZFSLIBDIR)


install-mountd: $(MOUNTDBINDIR)/mountd

$(MOUNTDBINDIR)/mountd: $(OBJDIR)/$(MOUNTDOBJDIR)/mountd
	install -b $(OBJDIR)/$(MOUNTDOBJDIR)/mountd $(MOUNTDBINDIR)



install: install-share

install-share: $(SHAREBINDIR)/share $(SHAREMANDIR)/share.1.gz

$(SHAREBINDIR)/share: share
	install share $(SHAREBINDIR)

$(SHAREMANDIR)/share.1.gz: share.man
	gzip <share.man >$(SHAREMANDIR)/share.1.gz


clean:
	rm -f *~ *.o share \#* core

distclean: clean
	rm -f *.patch mountd libzfs.so.4

push: distclean
	git add -A && git commit -a && git push
