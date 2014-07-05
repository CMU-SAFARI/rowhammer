#!sh  
# FILE: make_buildnum.sh  
version="`sed  's/^ *//' major_version`"  
old="`sed  's/^ *//' build.number` +1"  
echo $old | bc > build.number.temp  
mv build.number.temp build.number  
#versión..  
echo "$version`sed  's/^ *//' build.number` - `date`" > version.number  
#header  
echo "#ifndef BUILD_NUMBER_STR" > build_number.h  
echo "#define BUILD_NUMBER_STR \"`sed  's/^ *//' build.number`\"" >> build_number.h  
echo "#endif" >> build_number.h  
  
echo "#ifndef VERSION_STR" >> build_number.h  
echo "#define VERSION_STR \"$version`sed  's/^ *//' build.number` - `date`\"" >> build_number.h  
echo "#endif" >> build_number.h  
  
echo "#ifndef VERSION_STR_SHORT" >> build_number.h  
echo "#define VERSION_STR_SHORT \"$version`sed  's/^ *//' build.number`\"" >> build_number.h  
echo "#endif" >> build_number.h
