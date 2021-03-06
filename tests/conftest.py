from contextlib import ExitStack
from typing import Optional
from pathlib import Path
import shutil

import pytest

from tests import scoped_dds, DDSFixtureParams


@pytest.yield_fixture
def dds(request, tmp_path: Path, worker_id: str, scope: ExitStack):
    test_source_dir = Path(request.fspath).absolute().parent
    test_root = test_source_dir

    # If we are running in parallel, use a unique directory as scratch
    # space so that we aren't stomping on anyone else
    if worker_id != 'master':
        test_root = tmp_path / request.function.__name__
        shutil.copytree(test_source_dir, test_root)

    project_dir = test_root / 'project'
    # Check if we have a special configuration
    if hasattr(request, 'param'):
        assert isinstance(request.param, DDSFixtureParams), \
            ('Using the `dds` fixture requires passing in indirect '
            'params. Use @dds_fixture_conf to configure the fixture')
        params: DDSFixtureParams = request.param
        project_dir = test_root / params.subdir

    # Create the instance. Auto-clean when we're done
    yield scope.enter_context(
        scoped_dds(test_root, project_dir, request.function.__name__))


@pytest.fixture
def scope():
    with ExitStack() as scope:
        yield scope


def pytest_addoption(parser):
    parser.addoption(
        '--test-deps',
        action='store_true',
        default=False,
        help='Run the exhaustive and intensive dds-deps tests')


def pytest_configure(config):
    config.addinivalue_line(
        'markers', 'deps_test: Deps tests are slow. Enable with --test-deps')


def pytest_collection_modifyitems(config, items):
    if config.getoption('--test-deps'):
        return
    for item in items:
        if 'deps_test' not in item.keywords:
            continue
        item.add_marker(
            pytest.mark.skip(
                reason=
                'Exhaustive deps tests are slow and perform many Git clones. Use --test-deps to run them.'
            ))
