#!/usr/bin/env python3
"""Build the opt-in synthetic host after building the Harmony native core."""
import importlib.util
from pathlib import Path
APP=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('harmony_build',APP/'scripts/build.py')
build=importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)
_, _, xmake, env=build.environment()
build.run([xmake,'-P',APP/'native','-y','crossdesk_harmony_mobile_fixture'],cwd=APP/'native',env=env)
