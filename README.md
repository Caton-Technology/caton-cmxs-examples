# Caton Media XStream SDK Examples

The Caton Media XStream SDK provides a seamless and efficient solution for connecting to Caton Media XStream. This empowers developers to effortlessly send and receive real-time streams across multiple endpoints. With the reliability of SLA-guaranteed services offered by Caton Media XStream, application developers can enhance users' Quality of Experience (QoE) significantly.

<center>
<img src="https://global.caton.cloud/storage/cmxs_imgs/cmxs.png" />
</center>

Here is some example codes for showing how to use SDK.

The examples is based on CMXS SDK. CMXS SDK can be got at <a href="https://github.com/Caton-Technology/cmxs-sdk">CMXS SDK Github</a> . The system requirement is the same as CMXS SDK's.

To run these examples, please visit <a href="https://caton.cloud/console/xstream">Caton Media XStream</a> and get the parameters:

- key
- device id

and the server please use "https://caton.cloud".

Please contact with us if you have any questions. Our email is github-cmxs-examples@catontechnology.com


## VLC access plugin example

VLC access plugin example shows how to write a VLC access plugin by SDK.

VLC 3.0.16 is required.  The source code can be got from https://github.com/videolan/vlc . The binary can be got from https://www.videolan.org/vlc/releases/3.0.16.html .


### Build the example

We can use CMakeList.txt to generate a makefile and make it.

#### On Windows

Firstly, establish the environment by run vcvarsall.bat

secondly, you can generate makefile as:

```
 cmake -G"NMake Makefiles" -S . -B /path/to/out \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_INC_DIR_VLC=/path/to/vlc/includ/plugins] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_LIB_DIR_VLC=/path/to/vlc/lib]
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output]

```
then run nmake to build it.


#### On macOS

you can generate makefile as:

```
 cmake -S . -B /path/to/out \
    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
    [-DEXAMPLE_INC_DIR_VLC=/path/to/vlc/includ/plugins] \
    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
    [-DEXAMPLE_LIB_DIR_VLC=/path/to/vlc/lib]
    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
    [cross compile options]
```

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

## OBS plugin example

OBS plugin example shows how to write OBS plugins by SDK. The example provides one source plugin and one output plugin. The CMXS Source plugin receive CMXS video and audio in OBS. The CMXS Output plugin transmits OBS video and audio to CMXS.
 

### Build the example

Please download <a href="https://github.com/obsproject/obs-plugintemplate">obs plugin template</a> and copy the cmxs plugin code to template directory. Follow the OBS plugin template build steps to build and install the plugins.


### Run the example

#### On Windows

1. Copy the cmxssdk.dll and obs-cmxs.dll to the OBS plugin path (normally Program Files\obs-studio\obs-plugins\64bit).

2. run OBS.

3. Config global parameters.

    "Tool" menu -> Open CMXS settings, fill "Server address" and "DeviceId"

4. Config output parameters.

    "Tool" menu -> Open CMXS settings, fill "Key", check the "Enable Streaming" checkbox if you want to send data immediately.

5. Config source parameters.

    Right click "source" in OBS main UI, select "add" -> "CMXS source". Fill "Internal Port used by plugin" with a unique port. Fill "key".


#### On macOS

1. Install the cmxssdk, you can copy the dylib files to /path/to/obs/Contents/Frameworks/

2. Copy obs-cmxs.plugin directory to ~/Library/Application Support/obs-studio/plugins/

3. Run OBS.

4. Config global parameters.

    "Tool" menu -> Open CMXS settings, fill "Server address" and "DeviceId"

5. Config output parameters.

    "Tool" menu -> Open CMXS settings, fill "Key" and select NIC if necessary, check the "Enable Streaming" checkbox if you want to send data immediately.

6. Config source parameters.

    Right click "source" in OBS main UI, select "add" -> "CMXS source". Fill "Internal Port used by plugin" with a unique port. Fill "key" and select NIC if necessary.

