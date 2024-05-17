# Caton Media XStream SDK Examples

The Caton Media XStream SDK provides a seamless and efficient solution for connecting to Caton Media XStream. This empowers developers to effortlessly send and receive real-time streams across multiple endpoints. With the reliability of SLA-guaranteed services offered by Caton Media XStream, application developers can enhance users' Quality of Experience (QoE) significantly.

<center>
<img src="https://global.caton.cloud/storage/cmxs_imgs/cmxs.png" />
</center>

Here is some example codes for showing how to use SDK.

The examples is based on CMXS SDK. CMXS SDK can be got at <a href="https://github.com/caton-sdk/cmxs-sdk">CMXS SDK Github</a> . The system requirement is the same as CMXS SDK's.

To run these examples, please visit <a href="https://caton.cloud/console/xstream">Caton Media XStream</a> and get the parameters:

- key
- device id

and the server please use "https://caton.cloud".

Please contact with us if you have any questions. Our email is github-cmxs-examples@catontechnology.com


## C Interface examples

C Interface examples shows how to use the C interface.

There are two examples.

The example_c.c shows the simple usage of C interface.

The example_c_select.c shows how to use selector in receiver.


### Build the examples

We can use CMakeList.txt to generate a makefile and make it.

#### On linux based OS

you can generate makefile as:

 cmake -S . -B /path/to/cmake/output \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
    [cross compile options]

 then run make to build it.


#### On Windows:


 Firstly, establish the environment by run vcvarsall.bat

 secondly, you can generate makefile as:

 cmake -G"NMake Makefiles" -S . -B /path/to/cmake/output \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
    [cross compile options]

then run nmake to build it.

### Run the examples

 * For sending data:

  ./cmxs_c_example -s https://hello.caton.cloud -d hello_device -k hello_key -m send

 * For receiving data:

   simple example:

  ./cmxs_c_example -s https://hello.caton.cloud -d hello_device2 -k hello_key2 -m receive

   selector example:

  ./cmxs_c_example_select -s https://hello.caton.cloud -d hello_device2 -k hello_key2


## C++ Interface examples

C++ Interface example shows how to use the C++ interface.

There are two examples.

The example_cpp.cpp shows the simple usage of C interface.

The example_cpp_select.cpp shows how to use selector in receiver.


### Build the examples

We can use CMakeList.txt to generate a makefile and make it.

#### On linux based OS

you can generate makefile as:

 cmake -S . -B /path/to/cmake/output \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
    [cross compile options]

 then run make to build it.

#### On Windows

Firstly, establish the environment by run vcvarsall.bat

secondly, you can generate makefile as:

 cmake -G"NMake Makefiles" -S . -B /path/to/cmake/output \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
    [cross compile options]

then run nmake to build it.

### Run the examples

 * For sending data:

  ./cmxs_cpp_example -s https://hello.caton.cloud -d hello_device -k hello_key -m send

 * For receiving data:

   simple example:

  ./cmxs_cpp_example -s https://hello.caton.cloud -d hello_device2 -k hello_key2 -m receive

   selector example:

  ./cmxs_cpp_example_select -s https://hello.caton.cloud -d hello_device2 -k hello_key2



## VLC access plugin example

VLC access plugin example shows how to write a VLC access plugin by SDK.

VLC 3.0.16 is required.


### Build the example

We can use CMakeList.txt to generate a makefile and make it.

#### On Windows

Firstly, establish the environment by run vcvarsall.bat

secondly, you can generate makefile as:

 cmake -G"NMake Makefiles" -S . -B /path/to/out \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_INC_DIR_VLC=/path/to/vlc/includ/plugins] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_LIB_DIR_VLC=/path/to/vlc/lib]
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \

then run nmake to build it.


#### On macOS

you can generate makefile as:
 cmake -S . -B /path/to/out \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_INC_DIR_VLC=/path/to/vlc/includ/plugins] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_LIB_DIR_VLC=/path/to/vlc/lib]
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
    [cross compile options]

 then run make to build it.

### Run the example

#### On Windows

1. Copy the built libaccess_cmxs_plugin to the VLC plugin path.
2. Copy cmxssdk.dll to %WINDOWS%\system32\
3. run VLC.
4. open media.

"Media" menu -> Open Network..., use the url: cmxs://hello.caton.cloud[?[device=xx&][key=xx&][data_len=xx]]]

the parameters in [] also can be set in setting dialog. For set them: "Tools" menu -> Preference -> All -> Input/Codec -> Access modles -> cmxs, then fill the needed parameters.

#### On macOS

1. Copy the built libaccess_cmxs_plugin to the VLC plugin path.
2. run VLC.
3. open media.

"File" menu -> Open Network..., use the url: cmxs://hello.caton.cloud[?[device=xx&][key=xx&][data_len=xx]]
the parameters in [] also can be set in setting dialog. For set them: "VLC media player" menu -> Preference -> Show All -> Input/Codec -> Access modles -> cmxs, then fill the needed parameters.
