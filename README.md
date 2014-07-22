chromium-webcl
==============

#Introduction:
This branch is prototype the WebCL in Chromium with single process enabled. It could be run on Windows, Ubuntu and Android.


#Prerequest:
1. OpenCL.

Install the intel OpenCL SDK (https://software.intel.com/en-us/articles/intel-sdk-for-opencl-applications-2014-beta-release-notes).

2. Other dependency from Chromium.

Please follow the upstream wiki (http://www.chromium.org/Home) to install the prerequested library. 


#Build the content shell with WebCL(take android as example):
* Make root folder.

                  $mkdir chromium
                  $cd chromium

* Fetch chromium code.

                  $git clone https://github.com/junmin-zhu/chromium-webcl.git src
                  $git checkout -b webcl origin/webcl/35.0.1916.153

* Create gclient file

                  $vim .gclient

For Windows and Linux, use follow content:

    solutions = [{'custom_deps': {'build': None,
                  'build/scripts/command_wrapper/bin': None,
                  'build/scripts/gsd_generate_index': None,
                  'build/scripts/private/data/reliability': None,
                  'build/third_party/cbuildbot_chromite': None,
                  'build/third_party/gsutil': None,
                  'build/third_party/lighttpd': None,
                  'build/third_party/swarm_client': None,
                  'build/third_party/xvfb': None,
                  'build/xvfb': None,
                  'commit-queue': None,
                  'depot_tools': None,
                  'src': None,
                  'src/chrome/tools/test/reference_build/chrome_linux': None,
                  'src/chrome/tools/test/reference_build/chrome_mac': None,
                  'src/chrome/tools/test/reference_build/chrome_win': None,
                  'src/chrome_frame/tools/test/reference_build/chrome_win': None,
                  'src/content/test/data/layout_tests/LayoutTests': None,
                  'src/third_party/WebKit': 'https://github.com/junmin-zhu/blink-webcl.git@3bea45ec1ee8a3e606293d9326f5665838baae46',
                  'src/third_party/WebKit/LayoutTests': None,
                  'src/third_party/chromite': None,
                  'src/third_party/hunspell_dictionaries': None,
                  'src/third_party/pyelftools': None,
                  'src/webkit/data/layout_tests/LayoutTests': None},
    'name': '35.0.1916.153',
    'url': 'http://src.chromium.org/svn/releases/35.0.1916.153'}]

For android:

add "target_os = ['android']" at the end of .gclient.

* Fetch third parth code.

                  $gclient sync.

* Build the code.

                  $. build/android/envsetup.sh
                  $android_gyp
                  $ninja -C out/Release -j10 content_shell_apk


#Install and Run:
                  $cd out/Release
                  $adb install -r apks/ContentShell.apk



