#!/bin/bash
set -euo pipefail

TARGET=build/citrus
TIMEOUT=$1
OUT_PREFIX=$2
SUBJ_DIR=$3
FUNC_COMP=__none.txt
#USE_FUNC_COMP=$2
#FUNC_COMP=func_comp/tinyxml2.txt
#if [[ "${USE_FUNC_COMP^^}" == "NO" ]]; then
#  FUNC_COMP=__none.txt
#fi

TRANS_UNIT=${SUBJ_DIR}/tinyxml2.cpp
OBJ_DIR=${SUBJ_DIR}/build/CMakeFiles/tinyxml2.dir
SRC_DIR=${SUBJ_DIR}

ls ${TRANS_UNIT} > /dev/null
ls ${OBJ_DIR} > /dev/null
ls ${SRC_DIR} > /dev/null

MAX_DEPTH=1
XTRA_LD=""

${TARGET} ${TRANS_UNIT} \
  --obj-dir ${OBJ_DIR} \
  --src-dir ${SRC_DIR} \
  --max-depth ${MAX_DEPTH} \
  --fuzz-timeout ${TIMEOUT} \
  --xtra-ld "${XTRA_LD}" \
  --out-prefix ${OUT_PREFIX} \
  --func-comp ${FUNC_COMP}
