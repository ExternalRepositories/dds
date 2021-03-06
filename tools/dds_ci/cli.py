from argparse import ArgumentParser

from dds_ci import paths


def add_tc_arg(parser: ArgumentParser, *, required=True) -> None:
    parser.add_argument(
        '--toolchain',
        '-T',
        help='The DDS toolchain to use',
        required=required)


def add_dds_exe_arg(parser: ArgumentParser, *, required=True) -> None:
    parser.add_argument(
        '--exe',
        '-e',
        help='Path to a DDS executable to use',
        required=required)
