import pytest
import subprocess

from tests import DDS, DDSFixtureParams, dds_fixture_conf, dds_fixture_conf_1

dds_conf = dds_fixture_conf(
    DDSFixtureParams(ident='git-remote', subdir='git-remote'),
    DDSFixtureParams(ident='no-deps', subdir='no-deps'),
)


@dds_conf
def test_deps_build(dds: DDS):
    dds.catalog_import(dds.source_root / 'catalog.json')
    assert not dds.repo_dir.exists()
    dds.build()
    assert dds.repo_dir.exists(), '`Building` did not generate a repo directory'


@dds_fixture_conf_1('use-remote')
def test_use_nlohmann_json_remote(dds: DDS):
    dds.catalog_import(dds.source_root / 'catalog.json')
    dds.build(apps=True)

    app_exe = dds.build_dir / f'app{dds.exe_suffix}'
    assert app_exe.is_file()
    subprocess.check_call([str(app_exe)])
