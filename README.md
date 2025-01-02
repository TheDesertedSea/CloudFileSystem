# 云文件系统
设计并实现一个基于FUSE(File system in user space)的文件系统，将小文件存储于SSD，将大文件分块、去重后存储于对象
存储上。 

主要工作： 
- 基于FUSE，实现一系列VFS接口，包括getattr, getxattr, setxattr, mkdir, mknod, open, read, write, release, opendir, 
readdir, init, destroy, access, utimens, chmod, link, symlink, readlink, unlink, rmdir, truncate 
- 设计并实现小文件存储于SSD，大文件存储于对象存储，以及两者之间的自动迁移 
- 使用Rabin-Fingerprint 算法对大文件进行分块，并使用MD5计算块的哈希值作为对象存储的键 
- 设计并实现快照功能，包括快照的create、restore、list、delete、install、uninstall 
- 设计并实现一个write-back的缓存层，缓存对象存储上的文件块到SSD；使用LRU作为替换策略
