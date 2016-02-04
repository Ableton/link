#!/usr/bin/env python

from subprocess import call

import argparse
import logging
import os
import shutil
import sys


def parse_args():
  arg_parser = argparse.ArgumentParser()

  arg_parser.add_argument(
    '-t', '--target',
    help='Target to test')

  arg_parser.add_argument(
    '--valgrind', help='Run with Valgrind',
    action='store_true')

  return arg_parser.parse_args(sys.argv[1:])


def get_system_exe_extension():
  # Should return 'win32' even on 64-bit Windows
  if sys.platform == 'win32':
    return '.exe'
  else:
    return ''


def find_exe(name, path):
  for root, dirs, files in os.walk(path):
    if name in files:
      return os.path.join(root, name)


def build_test_exe_args(args, build_dir):
  if args.target is None:
    logging.error('Target not specified, please use the --target option')
    return None

  test_exe = find_exe(args.target + get_system_exe_extension(), build_dir)
  if not os.path.exists(test_exe):
    logging.error('Could not find test executable for target {}, '
                  'did you forget to build?'.format(args.target))
  else:
    logging.debug('Test executable is: {}'.format(test_exe))

  test_exe_args = []

  if args.valgrind is not None:
        if sys.platform == 'linux2':
            test_exe_args = ["valgrind", "--leak-check=full", "--show-reachable=yes",
            "--gen-suppressions=all", "--error-exitcode=1",
            "--suppressions=../scripts/memcheck.supp"]
        else:
            logging.error('Valgrind not supported on Windows or Mac.')
        test_exe_args.append(test_exe)

  return test_exe_args


def run_tests(args):
  scripts_dir = os.path.dirname(os.path.realpath(__file__))
  root_dir = os.path.join(scripts_dir, os.pardir)
  build_dir = os.path.join(root_dir, 'build')
  if not os.path.exists(build_dir):
    logging.error('Build directory not found, did you forget to run the configure.py script?')
    return 2

  os.chdir(build_dir)

  test_exe_args = build_test_exe_args(args, build_dir)
  if test_exe_args is None:
    return 1


  logging.info(test_exe_args)
  logging.info('Running Tests for {}'.format(args.target))
  return call(test_exe_args)


if __name__ == '__main__':
  logging.basicConfig(format='%(message)s', level=logging.INFO, stream=sys.stdout)
  sys.exit(run_tests(parse_args()))
