# sharedb

The files in this repo can be used to implement a DB-based NFS
exports & ZFS sharenfs support for FreeBSD.

Author: Peter Eriksson <pen@lysator.liu.se>

Version: 1.1 (2023-07-23)

The project consists of three parts:

1. share.c - a small CLI utility to update/view sharedb files
2. zfs-sharedb.patch - a patch for libshare in ZFS to make it update DB files
3. mountd.patch - a patch for mountd to make it read DB files


