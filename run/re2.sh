#!/bin/bash
set -euo pipefail

TARGET=build/citrus
TIMEOUT=$1
OUT_PREFIX=$2
SUBJ_DIR=$3
#USE_FUNC_COMP=$2
#FUNC_COMP=func_comp/re2.txt
#if [[ "${USE_FUNC_COMP^^}" == "NO" ]]; then
#  FUNC_COMP=__none.txt
#fi

TRANS_UNIT=${SUBJ_DIR}/re2/all.cpp
OBJ_DIR=${SUBJ_DIR}/build/CMakeFiles/re2.dir/
SRC_DIR=${SUBJ_DIR}/re2

ls ${TRANS_UNIT} > /dev/null
ls ${OBJ_DIR} > /dev/null
ls ${SRC_DIR} > /dev/null

MAX_DEPTH=2
XTRA_LD="-lpthread"

${TARGET} ${TRANS_UNIT} \
  --obj-dir ${OBJ_DIR} \
  --src-dir ${SRC_DIR} \
  --max-depth ${MAX_DEPTH} \
  --fuzz-timeout ${TIMEOUT} \
  --xtra-ld "${XTRA_LD}" \
  --out-prefix ${OUT_PREFIX} \
  --func-comp ${FUNC_COMP}
