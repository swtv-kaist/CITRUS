
from pathlib import Path
import subprocess
import os
import glob, shutil
import sys
import random


def n_last_lines(path, n):
    with open(path, 'r') as file:
        lines = file.readlines()
        return [line.strip() for line in lines][-n:]


def make_executable(path):
    mode = os.stat(path).st_mode
    mode |= (mode & 0o444) >> 2    # copy R bits to X
    os.chmod(path, mode)


def write_to_file(filename, insts, allow_fail=False):
    with open(filename, 'w') as cfile:
        print('#!/bin/bash', file=cfile)
        if not allow_fail:
            print('set -euo pipefail\n', file=cfile)
        for inst in insts:
            inst = inst.lstrip('// ')
            print(inst, file=cfile)
    make_executable(filename)


def combine_compile_instructions(compile_file, run_file, replay_file):
    compile_cmds = []
    run_cmds = []
    repl_cmds = []
    for root, dirs, files in os.walk('.'):
        valids = [x for x in files if '.cpp' in x]
        for name in valids:
            path = os.path.join(root, name)
            lines = n_last_lines(path, 8)
            compile_cmds.extend(lines[-2:])
            repl_cmds.extend([lines[-5]])
            run_cmds.extend([lines[-8]])

    write_to_file(compile_file, compile_cmds)
    write_to_file(run_file, run_cmds, True)
    write_to_file(replay_file, repl_cmds, True)
    print('DONE!', compile_file, run_file, replay_file)


def combine_libfuzzer_commands():
    compile_file = 'tst_compile.sh'
    run_file = 'tst_run.sh'
    repl_file = 'tst_repl.sh'
    combine_compile_instructions(compile_file, run_file, repl_file)


def split_as_individuals():
    with open('out_valid.cpp', 'r') as file:
        lines = file.readlines()
        lines = [line.rstrip() for line in lines]
    includes = []
    tc = []
    tcs = []
    tc_on = False
    for line in lines:
        if line.startswith('#include'):
            includes.append(line)
        elif tc_on:
            if line.startswith('}'):
                tc_on = False
                tcs.append(tc)
            else:
                tc.append(line)
        elif line.startswith('TEST('):
            tc_on = True
            tc = []

    print(includes)
    print(tcs)


def execute_cmd(cmd, store_output=False):
    cwd = os.getcwd()
    proc = subprocess.Popen(cmd, cwd=cwd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if not store_output:
        exit_code = proc.wait(30)
        return []

    output = []
    with proc.stdout:
        for line in iter(proc.stdout.readline, b''):
            line = line.decode("utf-8")
            line = line.rstrip()
            output.append(line)
    exit_code = proc.wait(30)
    return output


CLEAN_CMD='find /home/robert/CLionProjects/SWTV-CXXFOOZZ/_test_real/re2 -name "*.gcda" -exec rm -f {} \;'
MEASURE_CMD='gcovr -r /home/robert/CLionProjects/SWTV-CXXFOOZZ/_test_real/re2 --branch -s /home/robert/CLionProjects/SWTV-CXXFOOZZ/_test_real/re2/build_libfuzzer/CMakeFiles/re2.dir/ --gcov-executable gcov_for_clang.sh'


def replay_all_drivers():
    with open('tst_repl.sh', 'r') as file:
        lines = file.readlines()
        lines = [line.strip() for line in lines]
        lines = [line for line in lines if line != '#!/bin/bash']
    insts = []
    for idx, line in enumerate(lines):
        insts.append(CLEAN_CMD + ' &> /dev/null')
        insts.append('timeout 60s ' + line + ' &> /dev/null')
        insts.append(MEASURE_CMD + ' | tail -n2')

    write_to_file('tst_repl_combined.sh', insts, True)
    print('To use: ./tst_repl_combined.sh')

def replay_by_drivers(take, repeat):
    with open('tst_repl.sh', 'r') as file:
        lines = file.readlines()
        lines = [line.strip() for line in lines]
        lines = [line for line in lines if line != '#!/bin/bash']

    insts = []
    for _rep in range(repeat):
        filename = 'tst_repl_' + str(_rep) + '.sh'
        sampled = random.sample(lines, take)
        sampled = ['timeout 60s ' + s for s in sampled]
        write_to_file(filename, sampled, True)

        insts.append(CLEAN_CMD + ' &> /dev/null')
        insts.append('./' + filename + ' &> /dev/null')
        insts.append(MEASURE_CMD + ' | tail -n2')

    write_to_file('tst_repl_combined.sh', insts, True)
    print('To use: ./tst_repl_combined.sh')


def usage():
    print('To use: python3 batch_libfuzzer.py [gen|repl_all|repl X X]')
    exit(0)


if __name__ == '__main__':
    args = sys.argv
    if len(args) < 2:
        usage()

    cmd = args[1]
    if cmd == 'gen':
        combine_libfuzzer_commands()
    elif cmd == 'repl':
        if len(args) < 4:
            usage()
        take = int(args[2])
        repeat = int(args[3])
        replay_by_drivers(take, repeat)
    elif cmd == 'repl_all':
        replay_all_drivers()
    else:
        usage()
