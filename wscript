#
# Pebble SDK build script
#
import os
top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


def build(ctx):
    # Sprachressourcen + Text-IDs aus i18n/*.json erzeugen
    import sys
    sys.path.insert(0, ctx.path.find_dir('tools').abspath())
    import gen_strings
    gen_strings.generate(ctx.path.abspath())

    ctx.load('pebble_sdk')

    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        if os.environ.get('STAMINA_DEMO'):
            ctx.env.append_value('DEFINES', 'DEMO')  # Beispieldaten für Screenshots
        # src/shared: Bewegungserkennung, von App und Hintergrund-Worker genutzt
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob(['src/c/**/*.c', 'src/shared/**/*.c']),
                      target=app_elf, bin_type='app')
        worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob(['worker_src/c/**/*.c', 'src/shared/**/*.c']),
                      target=worker_elf, bin_type='worker')
        binaries.append({'platform': platform, 'app_elf': app_elf, 'worker_elf': worker_elf})
    ctx.env = cached_env

    # Das SDK benennt die .pbw nach dem Projektordner (pebble install erwartet diesen
    # Namen). Für den Store zusätzlich eine Kopie als stamina.pbw ablegen.
    def copy_bundle(ctx):
        import shutil
        src = ctx.path.get_bld().find_node(ctx.env.BUNDLE_NAME)
        if src:
            shutil.copyfile(src.abspath(), os.path.join(ctx.path.get_bld().abspath(), 'stamina.pbw'))
    ctx.add_post_fun(copy_bundle)

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js', 'src/pkjs/**/*.json']),
                   js_entry_file='src/pkjs/index.js')
