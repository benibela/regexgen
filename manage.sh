#/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/../../../manageUtils.sh

githubProject regexgen

BASE=$HGROOT/programs/data/regexgen

case "$1" in
mirror)
  syncHg  
;;
esac
