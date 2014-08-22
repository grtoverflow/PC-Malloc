supported kernel version: linux-2.6.32-71.el6.
supported architecture: Intel Xeon E7400-series

Usage:
1.Download source code for kernel linux-2.6.32-71.el6.
2.Copy the patch file(kernel-2.6.32-71.el6_pagecolor.patch) to the linux source code directory.
    $ cp kernel-2.6.32-71.el6_pagecolor.patch /your_kernel_dir/linux-2.6.32-71.el6/
3.Patching.
    $ patch -p1 < ./kernel-2.6.32-71.el6_pagecolor.patch
3.Re-compile your kernel.
    $ make mrproper
    $ make bzImage
    $ make dep
    $ make modules
    $ make modules_install
    $ make install
4.If you want to remove the patch, please typing:
    $ patch -p1 -R < ./kernel-2.6.32-71.el6_pagecolor.patch
