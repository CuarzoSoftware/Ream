project(
    'Ream',
    'c','cpp',
    version : run_command('cat', files('./VERSION'), check : false).stdout().strip(),
    meson_version: '>= 0.62.0',
    default_options: [
        'warning_level=2',
        'buildtype=release',
        'cpp_std=c++23'])

VERSION_MAJOR = meson.project_version().split('.')[0]
VERSION_MINOR = meson.project_version().split('.')[1]
VERSION_PATCH = meson.project_version().split('.')[2]
VERSION_BUILD = run_command('cat', './BUILD', check : false).stdout()

HEADERS_INSTALL_PATH = join_paths(get_option('prefix'), get_option('includedir'), 'CZ/Ream')

# -------------- DEPENDENCIES --------------

cpp             = meson.get_compiler('cpp')
pkg             = import('pkgconfig')
cz_core_dep     = dependency('cz-core')
cz_skia_dep     = dependency('cz-skia')
wayland_egl_dep = dependency('wayland-egl')
egl_dep         = dependency('egl', version: '>= 1.5')
gles2_dep       = dependency('glesv2', version: '>= 3.2')
gbm_dep         = dependency('gbm', version: '>= 23.2.1')
boost_dep       = dependency('boost', modules : ['container'])
drm_dep         = dependency('libdrm', version: '>= 2.4.113')
drm_headers_dep = drm_dep.partial_dependency(compile_args: true, includes: true)


# -------------- HEADERS --------------

header_dirs = [
    ['src/CZ/Ream', ''],
    ['src/CZ/Ream/DRM', 'DRM'],
    ['src/CZ/Ream/RS', 'RS'],
    ['src/CZ/Ream/GL', 'GL'],
    ['src/CZ/Ream/WL', 'WL'],
    ['src/CZ/Ream/GBM', 'GBM'],
    ['src/CZ/Ream/EGL', 'EGL'],
    ['src/CZ/Ream/SK', 'SK']
]

foreach header_dir : header_dirs
    header_files = run_command('find', header_dir[0], '-type', 'f', '-name', '*.h', '-maxdepth', '1', check : false).stdout().strip().split('\n')
    install_headers(header_files, install_dir : join_paths(HEADERS_INSTALL_PATH, header_dir[1]))
endforeach

# -------------- LIBRARY --------------

cz_ream = library(
    'cz-ream',
    sources : run_command('find', './src/CZ', '-type', 'f', '-name', '*[.cpp,.c]', check : false).stdout().strip().split('\n'),
    include_directories : ['src'],
    dependencies : [
        egl_dep,
        wayland_egl_dep,
        gles2_dep,
        gbm_dep,
        drm_dep,
        cz_core_dep,
        cz_skia_dep,
        boost_dep
    ],
    soversion: VERSION_MAJOR,
    install : true)

cz_ream_dep = declare_dependency(
    dependencies: [drm_headers_dep, cz_skia_dep],
    include_directories : ['src', 'src/CZ/Ream'],
    link_with : cz_ream)

pkg.generate(
    cz_ream,
    name: 'cz-ream',
    description: 'C++ Library for Graphics Context and Buffer Management',
    version: meson.project_version(),
    filebase: 'cz-ream',
    libraries: [
        drm_headers_dep,
        cz_core_dep,
        cz_skia_dep
    ],
    subdirs:'CZ/Ream')

subdir('src/examples/ream')
