#!/usr/bin/env bash
export PATH=`git rev-parse --git-dir`/../tools/:$PATH
git clang-format-14 --extensions c,h -v
rc=$?
if [[ $rc == 33 ]]
then
 echo "Nothing needs to be reformatted!"
 exit 0
fi
for file in `git diff-index --cached --name-only HEAD` ; do
  git add "${file}"
done
exit 0
