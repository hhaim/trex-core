#!/bin/bash

export SETUP_DIR=trex07
logdir=/tmp/profiles_log
rm -rf $logdir
mkdir -p $logdir

for i in {1..1000} ; do
  echo $i
  ./run_regression --python2 --kill-running --no-daemon  –stl -t test_all_profiles &> ${logdir}/${i}.log
  echo $?
done

