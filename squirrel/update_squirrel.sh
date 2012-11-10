#!/bin/bash
#
# script to update squirrel source code from SQUIRREL3
#

# copy and rename files
cp ~/SQUIRREL3/COPYRIGHT ../simutrans/license_squirrel.txt
cp ~/SQUIRREL3/include/* .
cp ~/SQUIRREL3/squirrel/*.h squirrel/

cp ~/SQUIRREL3/squirrel/*.h   squirrel/
cp ~/SQUIRREL3/squirrel/*.cpp squirrel/

cd squirrel
# rename
for i in *.cpp; do mv "$i" "${i/.cpp}".cc; done
# replace include <sq*.h> by include "../sq*.h"
for i in *.cc; do gawk -f ../update_squirrel.awk "$i" > temp; mv temp "$i"; done
for i in *.h; do gawk -f ../update_squirrel.awk "$i" > temp; mv temp "$i"; done
cd ..

cp ~/SQUIRREL3/sqstdlib/*.h   sqstdlib/
cp ~/SQUIRREL3/sqstdlib/*.cpp sqstdlib/

cd sqstdlib
# rename
for i in *.cpp; do mv "$i" "${i/.cpp}".cc; done
# replace include <sq*.h> by include "../sq*.h"
for i in *.cc; do gawk -f ../update_squirrel.awk "$i" > temp; mv temp "$i"; done
cd ..

git diff -w > update_squirrel.diff

git checkout -- squirrel
git checkout -- sqstdlib
git checkout -- sq*.h

patch -p2 < update_squirrel.diff