#!/usr/bin/env python

import argparse
import logging
import os
import subprocess
import sys


def parse_args():
    arg_parser = argparse.ArgumentParser()

    arg_parser.add_argument(
        '-c', '--clang-format',
        default='clang-format-5.0',
        help='Path to clang-format executable')

    return arg_parser.parse_args(sys.argv[1:])


def parse_clang_xml(xml):
    for line in xml.splitlines():
        if line.startswith('<replacement '):
            return False
    return True


def check_files_in_path(args, path):
    logging.info('Checking files in {}'.format(path))
    errors_found = False

    for (path, dirs, files) in os.walk(path):
        for file in files:
            if file.endswith('cpp') or file.endswith('hpp') or file.endswith('.ipp'):
                file_absolute_path = path + os.path.sep + file
                clang_format_args = [
                    args.clang_format, '-style=file',
                    '-output-replacements-xml', file_absolute_path]

                try:
                    clang_format_output = subprocess.check_output(clang_format_args)
                except subprocess.CalledProcessError:
                    logging.error(
                        'Could not execute {}, try running this script with the'
                        '--clang-format option'.format(args.clang_format))
                    sys.exit(2)

                if not parse_clang_xml(clang_format_output):
                    logging.warn('{} has formatting errors'.format(file_absolute_path))
                    errors_found = True

    return errors_found


def check_formatting(args):
    if not os.path.exists('.clang-format'):
        logging.error('Script must be run from top-level project directory')
        return 2

    errors_found = False
    for path in ['examples', 'include', 'src']:
        if check_files_in_path(args, path):
            errors_found = True

    if errors_found:
        logging.warn(
            'Formatting errors found, please fix with clang-format -style=file -i')
    else:
        logging.debug('No formatting errors found!')

    return errors_found


if __name__ == '__main__':
    logging.basicConfig(format='%(message)s', level=logging.INFO, stream=sys.stdout)
    sys.exit(check_formatting(parse_args()))
