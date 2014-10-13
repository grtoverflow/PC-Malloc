Usage:
1.install papi
 you can get papi at http://icl.cs.utk.edu/papi/software/index.html
2.install pcmalloc
 $ make
 $ make install
3.before running your program, please run set_preload.sh to complete the environment configuration.
 $ ${pcmalloc_dir}/util/set_preload.sh 1
