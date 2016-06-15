from __future__ import print_function
import unittest
import argparse
import re
import tempfile
import subprocess
import threading
import os
import sys

lldb = ''
corerun = ''
sosplugin = ''
assembly = ''
fail_flag = 'fail_flag'
summary_file = 'summary'
timeout = 0

# helper functions


def runWithTimeout(cmd):
    d = {'process': None}

    def run():
        d['process'] = subprocess.Popen(cmd, shell=True)
        d['process'].communicate()

    thread = threading.Thread(target=run)
    thread.start()

    thread.join(timeout)
    if thread.is_alive():
        d['process'].terminate()
        thread.join()


# Test class
class TestSosCommands(unittest.TestCase):

    def do_test(self, command):
        global corerun
        global sosplugin

        try:
            os.unlink(fail_flag + '.lldb')
        except:
            pass

        cmd = (('%s -b -K "OnCrash.do" ' % lldb) +
               ("-O \"plugin load %s \" " % sosplugin) +
               ("-o \"script from testutils import run\" ") +
               ("-o \"script run('%s', '%s')\" " % (assembly, command)) +
               ("-o \"quit\" ") +
               (" -- %s %s > %s.log 2> %s.log.2" % (corerun, assembly,
                                                    command, command)))

        runWithTimeout(cmd)
        self.assertFalse(os.path.isfile(fail_flag))
        self.assertFalse(os.path.isfile(fail_flag + '.lldb'))

        try:
            os.unlink(fail_flag)
        except:
            pass
        try:
            os.unlink(fail_flag + '.lldb')
        except:
            pass

    def test_bpmd(self):
        self.do_test("t_cmd_bpmd")

    def test_clrstack(self):
        self.do_test("t_cmd_clrstack")

    def test_clrthreads(self):
        self.do_test("t_cmd_clrthreads")

    def test_clru(self):
        self.do_test("t_cmd_clru")

    def test_dumpclass(self):
        self.do_test("t_cmd_dumpclass")

    def test_dumpheap(self):
        self.do_test("t_cmd_dumpheap")

    def test_dumpil(self):
        self.do_test("t_cmd_dumpil")

    def test_dumplog(self):
        self.do_test("t_cmd_dumplog")

    def test_dumpmd(self):
        self.do_test("t_cmd_dumpmd")

    def test_dumpmodule(self):
        self.do_test("t_cmd_dumpmodule")

    def test_dumpmt(self):
        self.do_test("t_cmd_dumpmt")

    def test_dumpobj(self):
        self.do_test("t_cmd_dumpobj")

    def test_dumpstack(self):
        self.do_test("t_cmd_dumpstack")

    def test_dso(self):
        self.do_test("t_cmd_dso")

    def test_eeheap(self):
        self.do_test("t_cmd_eeheap")

    def test_eestack(self):
        self.do_test("t_cmd_eestack")

    def test_gcroot(self):
        self.do_test("t_cmd_gcroot")

    def test_ip2md(self):
        self.do_test("t_cmd_ip2md")

    def test_name2ee(self):
        self.do_test("t_cmd_name2ee")

    def test_pe(self):
        self.do_test("t_cmd_pe")

    def test_histclear(self):
        self.do_test("t_cmd_histclear")

    def test_histinit(self):
        self.do_test("t_cmd_histinit")

    def test_histobj(self):
        self.do_test("t_cmd_histobj")

    def test_histobjfind(self):
        self.do_test("t_cmd_histobjfind")

    def test_histroot(self):
        self.do_test("t_cmd_histroot")

    def test_sos(self):
        self.do_test("t_cmd_sos")

    def test_soshelp(self):
        self.do_test("t_cmd_soshelp")


def generate_report():
    global summary_file

    report = [{'name': 'TOTAL', True: 0, False: 0, 'complete': True}]
    fail_messages = []

    with open(summary_file, 'r') as summary:
        for line in summary:
            if line.startswith('new_suite: '):
                report.append({'name': line.split()[-1][2:], True: 0, False: 0,
                               'complete': False})
            elif line.startswith('True'):
                report[-1][True] += 1
            elif line.startswith('False'):
                report[-1][False] += 1
            elif line.startswith('Complete!'):
                report[-1]['complete'] = True
            elif line.startswith('!!! '):
                fail_messages.append(line.rstrip('\n'))

    for suite in report[1:]:
        report[0][True] += suite[True]
        report[0][False] += suite[False]
        report[0]['complete'] &= suite['complete']

    for line in fail_messages:
        print(line)

    print()
    print('=======================================')
    print('{:15} {:6} {:6} {:9}'.format('Test suite', 'Pass', 'Fail',
          'Completed'))
    print('---------------------------------------')
    for suite in report[1:]:
        print('{:15} {:>4}   {:>4}   {:>9}'.format(suite['name'], suite[True],
              suite[False], str(suite['complete'])))
    print()
    print('{:15} {:>4}   {:>4}   {:>9}'.format('TOTAL', report[0][True],
          report[0][False], str(report[0]['complete'])))
    print('=======================================')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--lldb', default='lldb')
    parser.add_argument('--corerun', default='')
    parser.add_argument('--sosplugin', default='')
    parser.add_argument('--assembly', default='test.exe')
    parser.add_argument('--timeout', default=120)
    parser.add_argument('unittest_args', nargs='*')

    args = parser.parse_args()

    lldb = args.lldb
    corerun = args.corerun
    sosplugin = args.sosplugin
    assembly = args.assembly
    timeout = int(args.timeout)
    print("lldb: " + lldb)
    print("corerun: " + corerun)
    print("sosplugin: " + sosplugin)
    print("assembly: " + assembly)
    print("timeout: " + str(timeout))

    try:
        os.unlink(summary_file)
    except:
        pass

    sys.argv[1:] = args.unittest_args
    suite = unittest.TestLoader().loadTestsFromTestCase(TestSosCommands)
    unittest.TextTestRunner(verbosity=2).run(suite)

    generate_report()
