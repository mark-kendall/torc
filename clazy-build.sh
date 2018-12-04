#!/bin/bash

# exclusions - based on clazy 1.3
# inefficient-qlist             various QVariant functions only return QVariantList (aka QList<QVariant>). Valid hits fixed.
# non-pod-global-static         doesn't fair well with factory classes
# ctor-missing-parent-argument  we simply don't need the QObject parenting in most cases (any?)
# qstring-allocations           soooo many

make clean
export CLAZY_CHECKS="level0,level1,level2,level3,no-inefficient-qlist,no-non-pod-global-static,no-ctor-missing-parent-argument,no-qstring-allocations"
clazy --list
TORC_FFMPEG=1 qmake -spec linux-clang QMAKE_CXX="clazy"
make -j8
