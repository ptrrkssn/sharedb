# sharedb 1.1 - Peter Eriksson <pen@lysator.liu.se>, 2023-07-23

The files in this repo can be used to implement DB-based (B-Tree)
NFS exports for ZFS and mountd for FreeBSD 13+.

It also allows "multiline" exports for ZFS (using ";" to separate the options).

There is a significant speed improvement for servers with many exported
ZFS filesystems. One test of "zfs share -a" on a server with 11000 filesystems
made the time go from 41 seconds to 2.7s.


The project consists of three parts:

1. share.c - a small CLI utility to update/view sharedb files
2. zfs-sharedb.patch - a patch for libshare in ZFS to make it update DB files
3. mountd.patch - a patch for mountd to make it read DB files


The patch to zfs enables updating of the database files when
you use:

  zfs set sharenfs="<options>" FILESYS

It will check if the DB-based Btree /etc/zfs/exports.db file exists,
and then use the new code automatically.

If it doesn't exist then it will use to old text-based /etc/zfs/exports file.

You can use ";" to separate multiline options per above like this:

  zfs set sharenfs="-sec=krb5;-sec=sys somehost.somewhere" FILESYS


The patch to mountd adds reading the DB files in preference to the text
versions if it finds then when scanning the list of exports sources.



As long as you don't have any "multiline" exports looking like this
in /etc/exports:

  /export/foo -sec=krb5
  /export/foo -sec=sys somehost.somewhere

then you can use "makemap" to populate DB files like this:

  makemap -f btree /etc/exports.db </etc/exports
  makemap -f btree /etc/zfs/exports.db </etc/zfs/exports

There is no need to use makemap for /etc/zfs/exports.db if you use the
zfs patch though.


The share CLI too can be used to view and update the DB files. It's still
a work in progress. Run it with "-h" for usage information. A man page will
be written soon...

- Peter




