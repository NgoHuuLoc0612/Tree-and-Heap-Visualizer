"""
setup.py — Tree & Heap Visualizer
Builds the pybind11 C++ extension: core_engine

Usage:
  python setup.py build_ext --inplace   # build .so next to server.py
  python setup.py install               # install into site-packages

Requirements:
  pip install pybind11 setuptools
"""

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import pybind11
import sys
import os

# ── Compiler flags ──────────────────────────────────────────
extra_compile = ["-O2", "-std=c++17", "-fvisibility=hidden"]
extra_link     = []

if sys.platform == "darwin":           # macOS
    extra_compile += ["-mmacosx-version-min=10.14"]
    extra_link    += ["-mmacosx-version-min=10.14"]
elif sys.platform == "win32":          # Windows
    extra_compile  = ["/O2", "/std:c++17"]   # MSVC flags
    extra_link     = []

# ── Extension definition ─────────────────────────────────────
core_engine = Extension(
    name="core_engine",
    sources=["core_engine.cpp"],
    include_dirs=[
        pybind11.get_include(),          # pybind11 headers
    ],
    language="c++",
    extra_compile_args=extra_compile,
    extra_link_args=extra_link,
)

# ── Custom build command (optional: verbose output) ──────────
class BuildExt(build_ext):
    def build_extensions(self):
        ct = self.compiler.compiler_type
        print(f"[setup.py] Compiler type: {ct}")
        print(f"[setup.py] Building core_engine extension…")
        super().build_extensions()
        print("[setup.py] Done ✓")

# ── Setup ────────────────────────────────────────────────────
setup(
    name="tree_heap_visualizer",
    version="2.0.0",
    description="Tree & Heap Visualizer — C++ core engine (pybind11)",
    ext_modules=[core_engine],
    cmdclass={"build_ext": BuildExt},
    python_requires=">=3.9",
    install_requires=[
        "pybind11>=2.10",
        "flask>=2.3",
        "flask-cors>=4.0",
        "flask-socketio>=5.3",
    ],
    zip_safe=False,
)
