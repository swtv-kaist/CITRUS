# CITRUS: C++ Unit Testing for Real-world Usage

## Introduction

CITRUS is an implementation of Unit-level Testing for C++ based on random method call sequence generation. 

CITRUS automatically generates test driver files for the target program `P`, each of which consists of various method calls
of `P`.  In
addition, CITRUS improves the test coverage of `P` further
by applying **libfuzzer** to change `P`’s state by mutating
arguments of the methods.

For more details, please refer to CITRUS technical paper.

## Requirements

CITRUS was tested running on Ubuntu 16.04, 18.04, 20.04. The requirements of CITRUS are:
1. LLVM/Clang++ 11.0.1,
1. LCOV coverage measurement tool (we used a [modified LCOV](https://github.com/henry2cox/lcov/tree/diffcov_initial) for CITRUS development),
1. CMake 3.15,
1. Python 3.

We provide a shell script to install all CITRUS requirements. (**root privilege required**)
```shell
./scripts/dep.sh
```


## Build Instruction

To build CITRUS is simply executing the build script
```shell
./scripts/build.sh
```
CITRUS will be built in `build` directory.


## Building CITRUS Subjects

We provide the target programs we use for our experiment at 
`replication` directory. For simplicity, you can execute the following shell script (from the CITRUS **root** project directory) to build all our experiments subjects.
```shell
./scripts/bootstrap.sh subjects             # to build in subjects dir
```

## Running CITRUS Method Call Sequence Generation

Currently CITRUS only supports command-line interface.
```shell
./build/citrus ${TRANS_UNIT} \
  --obj-dir ${OBJ_DIR} \
  --src-dir ${SRC_DIR} \
  --max-depth ${MAX_DEPTH} \
  --fuzz-timeout ${TIMEOUT} \
  --xtra-ld "${XTRA_LD}" \
  --out-prefix ${OUT_PREFIX}
```
For easier usage, we recommend to write separate shell script(s) to configure the command-line arguments as demonstrated in `run` directory. For example, to run CITRUS on `hjson` library:
```shell
./run/hjson.sh 43200 tc_hjson subjects/hjson-cpp          # 12 hours
```
where `tc_hjson` represents the target directory where the generated test cases will be put at, and `subjects/hjson-cpp` represents the `hjson` directory.

## Running CITRUS libfuzzer

Currently the libfuzzer stage must be manually triggered after the method call sequence generation. CITRUS writes the libfuzzer harness drivers in `out_libfuzzer` directory. Each driver has compilation instruction at the end of the file.

To ease the libfuzzer stage, we provide `batch_libfuzzer.py` script (i.e., CITRUS already puts this script in `out_libfuzzer` directory) to collect all compilation, running, and test case replaying instructions for libfuzzer stage.
```shell
# Compilation (from out_libfuzzer directory)
python3 batch_libfuzzer gen             # initializes the scripts
./tst_compile.sh                        # compile all harness drivers

# Running libfuzzer
./tst_run.sh                            # default: 5 mins each driver

# Replaying libfuzzer generated test cases
./tst_repl.sh                           
```

---
Developed by **SWTV Lab**, **KAIST**
