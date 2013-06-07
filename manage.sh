#/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/../../../manageUtils.sh

githubProject regexgen

BASE=$HGROOT/programs/data/regexgen

case "$1" in
mirror)
  syncHg  
;;
compile)
  g++ regexgen.cpp -o regexgen -I/usr/include/qt4/QtCore -I/usr/include/qt4/ -lQtCore
;;
test)
  ./regexgen < regexgen.test.in > /tmp/out
  diff /tmp/out regexgen.test.out
;;
esac
