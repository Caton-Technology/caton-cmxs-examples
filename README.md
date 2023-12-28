# ![GitHub set up](https://global.caton.cloud/storage/cmxs_imgs/logo3.jpg) Caton Media XStream SDK Examples

The Caton Media XStream SDK provides a seamless and efficient solution for connecting to Caton Media XStream. This empowers developers to effortlessly send and receive real-time streams across multiple endpoints. With the reliability of SLA-guaranteed services offered by Caton Media XStream, application developers can enhance users' Quality of Experience (QoE) significantly.

![GitHub set up](https://global.caton.cloud/storage/cmxs_imgs/cmxs.png)

Here is some example codes for showing how to use SDK.

To run these examples, please visit <a href="https://caton.cloud/console/xstream">Caton Media XStream</a> and get the parameters:

- key
- device id

and the server please use "https://caton.cloud".

Please contact with us if you have any questions. Our email is github-cmxs-examples@catontechnology.com


## C Interface example

C Interface example shows how to use the C interface.

### Build the example

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

### Run the example

 * For sending data:

  ./cmxs_c_example -s hello.caton.cloud -d hello_device -k hello_key -m send

 * For receiving data:

  ./cmxs_c_example -s hello.caton.cloud -d hello_device2 -k hello_key2 -m receive


## C++ Interface example

C++ Interface example shows how to use the C++ interface.

### Build the example

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

### Run the example

 * For sending data:

  ./cmxs_cpp_example -s hello.caton.cloud -d hello_device -k hello_key -m send

 * For receiving data:

  ./cmxs_cpp_example -s hello.caton.cloud -d hello_device2 -k hello_key2 -m receive



## VLC access plugin example

VLC access plugin example shows how to write a VLC access plugin by SDK.

### Build the example

We can use CMakeList.txt to generate a makefile and make it.

#### On linux based OS

you can generate makefile as:

 cmake -S . -B /path/to/out \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_INC_DIR_VLC=/path/to/vlc/includ/plugins] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_LIB_DIR_VLC=/path/to/vlc/lib]
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \


 then run make to build it.

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

### Run the example



1. Copy the built libaccess_cmxs_plugin to the VLC plugin path.
2. run VLC.
3. open media.

"Media" menu -> Open Network..., use the url: cmxs://[[server]?[device=xx&][key=xx&][data_len=xx]]

the parameters in [] also can be set in setting dialog. For set them: "Tools" menu -> Preference -> All -> Input/Codec -> Access modles -> cmxs, then fill the needed parameters.




