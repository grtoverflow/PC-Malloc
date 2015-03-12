
After you install NightWatch, follow these steps to install tcmalloc:

1. ./configure
2. Since tcmalloc contains several test programs, we need to modify the 'Makefile' to make
   these programs properly linked to NightWatch.
   Modify the 'Makefile' as the example 'example_Makefile' shows. For every place that need 
   modification, there will be a comment 'NIGHTWATCH ADD' as leading.
3. make
4. make install
