project(
    'Ream',
    'c','cpp',
    version : run_command('cat', files('./VERSION'), check : false).stdout().strip(),
    meson_version: '>= 0.62.0',
    default_options: [
        'warning_level=2',
        'buildtype=release',
        'cpp_std=c++20'])

VERSION_MAJOR = meson.project_version().split('.')[0]
VERSION_MINOR = meson.project_version().split('.')[1]
VERSION_PATCH = meson.project_version().split('.')[2]
VERSION_BUILD = run_command('cat', './BUILD', check : false).stdout()

HEADERS_INSTALL_PATH = join_paths(get_option('prefix'), get_option('includedir'), 'CZ/Ream')

# -------------- DEPENDENCIES --------------

cpp = meson.get_compiler('cpp')
pkg = import('pkgconfig')
drm_dep = dependency('libdrm', version: '>= 2.4.113')
drm_headers_dep = drm_dep.partial_dependency(compile_args: true, includes: true)
egl_dep = dependency('egl', version: '>= 1.5')
gles2_dep = dependency('glesv2', version: '>= 3.2')
skia_dep = dependency('cz-skia')
deps = [
    dependency('cz-core'),
    dependency('gbm', version: '>= 23.2.1'),
    skia_dep,
    gles2_dep,
    egl_dep,
    drm_dep
]

# -------------- HEADERS --------------

header_dirs = [
    ['./src/CZ/Ream', '']
]

foreach header_dir : header_dirs
    header_files = run_command('find', header_dir[0], '-type', 'f', '-name', '*.h', '-maxdepth', '1', check : false).stdout().strip().split('\n')
    install_headers(header_files, install_dir : join_paths(HEADERS_INSTALL_PATH, header_dir[1]))
endforeach

# -------------- LIBRARY --------------

cz_ream = library(
    'cz-ream',
    sources : run_command('find', './src/CZ', '-type', 'f', '-name', '*[.cpp,.c]', check : false).stdout().strip().split('\n'),
    include_directories : ['./src'],
    dependencies : deps,
    soversion: VERSION_MAJOR,
    install : true)

cz_ream_dep = declare_dependency(
    dependencies: [drm_headers_dep],
    include_directories : ['src', 'src/CZ/Ream'],
    link_with : cz_ream)

pkg.generate(
    cz_ream,
    name: 'cz-ream',
    description: 'C++ Library for Graphics Context and Buffer Management',
    version: meson.project_version(),
    filebase: 'Ream',
    subdirs:'CZ/Ream')

subdir('src/examples/ream')
