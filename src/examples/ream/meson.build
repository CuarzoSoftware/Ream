executable(
    'ream',
    sources : ['main.cpp', 'xdg-shell.c'],
    dependencies : [
        dependency('wayland-client'),
        dependency('wayland-egl'),
        skia_dep,
        gles2_dep,
        egl_dep,
        cz_ream_dep
    ],
    install : true)
