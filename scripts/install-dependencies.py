#!/usr/bin/env python

from subprocess import call

import argparse
import logging
import os
import shutil
import sys


class Packages(object):
  """Packages to be defined for various package managers. This is defined outside
  of the PackageManager class for easy mantainability."""

  """For Debian-based Linux systems"""
  APT = ['cmake', 'clang', 'ninja-build']

  """For Homebrew on Mac OS X"""
  HOMEBREW = ['cmake', 'ninja']


def package_manager_needs_sudo():
  # Currently only apt needs sudo access
  if sys.platform == 'linux2':
    return True
  else:
    return False


def parse_args():
  arg_parser = argparse.ArgumentParser()

  arg_parser.add_argument(
    '-n', '--dry-run',
    default=False, action='store_true',
    help='Don\'t actually install packages, but just show what commands would be run')

  arg_parser.add_argument(
    '--sudo',
    dest='sudo', action='store_true',
    help='Use sudo for package installation commands (default: %(default)s)')

  arg_parser.add_argument(
    '--no-sudo',
    dest='sudo', action='store_false',
    help='Don\'t use sudo for package installation commands')

  arg_parser.set_defaults(sudo=package_manager_needs_sudo())

  return arg_parser.parse_args(sys.argv[1:])


class PackageManager(object):
  def __init__(self, args):
    self.dry_run = args.dry_run

  def get_packages(self):
    return []

  def update(self):
    return False

  def install(self, packages):
    return False


class PackageManagerGenericUnix(PackageManager):
  def __init__(self, args):
    super(PackageManagerGenericUnix, self).__init__(args)
    self.use_sudo = args.sudo

  def get_package_manager_bin(self):
    return None

  def get_base_command_args(self):
    command_args = []
    if self.use_sudo:
      command_args.append('sudo')
    command_args.append(self.get_package_manager_bin())
    return command_args

  def update(self):
    command_args = self.get_base_command_args()
    command_args.append('update')
    if self.dry_run:
      logging.info('{}'.format(' '.join(command_args)))
      return True
    else:
      return call(command_args) == 0

  def install(self):
    command_args = self.get_base_command_args()
    command_args.append('install')
    for package in self.get_packages():
      command_args.append(package)
    if self.dry_run:
      logging.info('{}'.format(' '.join(command_args)))
      return True
    else:
      return call(command_args) == 0


class PackageManagerApt(PackageManagerGenericUnix):
  def __init__(self, args):
    super(PackageManagerApt, self).__init__(args)

  def get_package_manager_bin(self):
    return 'apt-get'

  def get_packages(self):
    return Packages.APT


class PackageManagerHomebrew(PackageManagerGenericUnix):
  def __init__(self, args):
    super(PackageManagerHomebrew, self).__init__(args)

  def get_package_manager_bin(self):
    return 'brew'

  def get_packages(self):
    return Packages.HOMEBREW


def get_package_manager(args):
  """Factory for creating the correct package manager for a given platform"""
  # Should return 'win32' even on 64-bit Windows
  if sys.platform == 'win32':
    # We don't support package managers on Windows
    return None
  elif sys.platform == 'darwin':
    # We only support homebrew on Mac
    return PackageManagerHomebrew(args)
  # Matches both 2.x and 3.x kernels
  elif sys.platform == 'linux2':
    # We only support Debian-based Linux systems
    return PackageManagerApt(args)


def install_dependencies(args):
  package_manager = get_package_manager(args)
  if package_manager is not None:
    if package_manager.dry_run:
      print 'dry-run option specified, no commands will be executed.'
      print 'The following commands would be run:'
      print

    if not package_manager.update():
      logging.error('Failed updating packages, cannot continue')
      return 1

    if not package_manager.install():
      logging.error('Failed installing packages')
      return 2

  return 0


if __name__ == '__main__':
  logging.basicConfig(format='%(message)s', level=logging.INFO, stream=sys.stdout)
  sys.exit(install_dependencies(parse_args()))
