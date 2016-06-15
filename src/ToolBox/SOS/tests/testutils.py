from __future__ import print_function
import lldb
import re
import inspect
import sys
import os
import importlib
from test_libsosplugin import fail_flag
from test_libsosplugin import summary_file

# for use in unit tests as test.FATAL / test.NONFATAL
FATAL = True
NONFATAL = False

failed = False


def assertCommon(passed, fatal):
    global summary_file
    global failed
    with open(summary_file, 'a+') as summary:
        print(bool(passed), file=summary)
        if (not passed):
            failed = True
            print('!!! test failed:', file=summary)
            for s in inspect.stack()[2:]:
                print("!!!  %s:%i" % (s[1], s[2]), file=summary)
                print("!!! %s" % s[4][0], file=summary)
                if re.match('\W*t_\w+\.py$', s[1]):
                    break

            if fatal:
                exit(1)


def assertTrue(x, fatal=True):
    passed = bool(x)
    assertCommon(passed, fatal)


def assertFalse(x, fatal=True):
    passed = not bool(x)
    assertCommon(passed, fatal)


def assertEqual(x, y, fatal=True):
    passed = (x == y)
    assertCommon(passed, fatal)


def assertNotEqual(x, y, fatal=True):
    passed = (x != y)
    assertCommon(passed, fatal)


def checkResult(res):
    if not res.Succeeded():
        print(res.GetOutput())
        print(res.GetError())
        exit(1)


def is_hexnum(s):
    try:
        int(s, 16)
        return True
    except ValueError:
        return False


def exec_and_find(commandInterpreter, cmd, regexp):
    res = lldb.SBCommandReturnObject()
    commandInterpreter.HandleCommand(cmd, res)
    checkResult(res)

    expr = re.compile(regexp)
    addr = None

    print(res.GetOutput())
    lines = res.GetOutput().splitlines()
    for line in lines:
        match = expr.match(line)
        if match:
            addr = match.group(1)
            break

    print("Found addr: " + str(addr))
    return addr


def stop_in_main(debugger, assembly):
    ci = debugger.GetCommandInterpreter()
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    res = lldb.SBCommandReturnObject()

    # Process must be stopped here while libcoreclr loading.
    # This test usually fails on release version of coreclr
    # since we depend on 'LoadLibraryExW' symbol present.
    assertEqual(process.GetState(), lldb.eStateStopped)

    ci.HandleCommand("bpmd " + assembly + " Test.Main", res)
    print(res.GetOutput())
    print(res.GetError())
    # interpreter must have this command and able to run it
    assertTrue(res.Succeeded())

    output = res.GetOutput()
    # Output is not empty
    # Should be at least 'Adding pending breakpoints...'
    assertTrue(len(output) > 0)

    process.Continue()
    # Process must be stopped here if bpmd works at all
    assertEqual(process.GetState(), lldb.eStateStopped)


def exit_lldb(debugger, assembly):
    ci = debugger.GetCommandInterpreter()
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    res = lldb.SBCommandReturnObject()

    process.Continue()
    # Process must exit
    assertEqual(process.GetState(), lldb.eStateExited)

    # Process must exit with zero code
    assertEqual(process.GetExitStatus(), 0)


def run(assembly, module):
    global fail_flag
    global summary_file
    global failed

    open(fail_flag, "a").close()
    with open(summary_file, 'a+') as summary:
        print('new_suite: %s' % module, file=summary)

    debugger = lldb.debugger

    debugger.SetAsync(False)
    target = lldb.target

    debugger.HandleCommand("process launch -s")
    debugger.HandleCommand("breakpoint set -n LoadLibraryExW")

    target.GetProcess().Continue()

    debugger.HandleCommand("breakpoint delete 1")
    #run the scenario
    print("starting scenario...")
    i = importlib.import_module(module)
    scenarioResult = i.runScenario(os.path.basename(assembly), debugger,
                                   target)

    if (target.GetProcess().GetExitStatus() == 0) and not failed:
        os.unlink(fail_flag)

    with open(summary_file, 'a+') as summary:
        print('Complete!', file=summary)
