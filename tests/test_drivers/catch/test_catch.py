from tests import DDS, dds_fixture_conf, DDSFixtureParams
from dds_ci import proc


@dds_fixture_conf(
    DDSFixtureParams('main', 'main'),
    DDSFixtureParams('custom-runner', 'custom-runner'),
)
def test_catch_testdriver(dds: DDS):
    dds.build(tests=True)
    test_exe = dds.build_dir / f'test/testlib/calc{dds.exe_suffix}'
    assert test_exe.exists()
    assert proc.run([test_exe]).returncode == 0
