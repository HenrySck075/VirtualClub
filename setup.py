import os
from setuptools import setup, Extension
from Cython.Build import cythonize
from Cython.Distutils.build_ext import build_ext


def get_pxd_paths(root_dir):
    """Finds all .pxd files that need to be compiled into native extensions."""
    paths = []
    for root, dirs, files in os.walk(root_dir):
        # Skip virtual environments or build directories
        if any(p in root for p in [".git", "build", "dist", "venv", ".github"]):
            continue
        for file in files:
            if file.endswith(".pyx"):
                paths.append(os.path.join(root, file))
    return paths

# Gather all your definition files
pyx_files = [
    Extension(
        name="fuse",
        sources=["lib/fuse.pyx"],
        extra_compile_args=['-D_FILE_OFFSET_BITS=64'],
        libraries=['fuse3'],
    )
]

setup(
    name="virtual_club",
    # Cythonize your .pxd files directly into compiled modules
    ext_modules=cythonize(
        pyx_files,
        compiler_directives={"language_level": "3"},
        exclude_failures=True
    ),
    cmdclass={"build_ext": build_ext},
)
