### EGSM
A module that enhances gmod with a thing that was asked for decades

* [Examples](https://github.com/devonium/EGSM/wiki/example_shaders)
* [Documentation](https://github.com/devonium/EGSM/wiki)
* [Discord](https://discord.gg/X2Ay3cgW8T)
 
### How to install
* Put [dll](https://github.com/devonium/EGSM/releases) (`gmsv_egsm_win32.dll` is for `main` branch and `gmsv_egsm_chromium_win32.dll`/`gmsv_egsm_chromium_win64.dll` for `x86-64`) in garrysmod/lua/bin
* Extract [shaders.zip](https://github.com/devonium/EGSM/releases) to garrysmod/
* APPEND `require((BRANCH == "x86-64" or BRANCH == "chromium" ) and "egsm_chromium" or "egsm")` to garrysmod/lua/menu/menu.lua

https://www.youtube.com/watch?v=hRg8DLqhnNY

### How to compile
[garrysmod_common](https://github.com/danielga/garrysmod_common)
```
git clone --recursive https://github.com/danielga/garrysmod_common
git clone --recursive https://github.com/danielga/garrysmod_common -b "x86-64-support-sourcesdk"
```

```
premake5 vs2017 --gmcommon="garrysmod_common_path" --autoinstall="gmod_lua_bin_path"
premake5 vs2017 --gmcommon="garrysmod_common_x86_64_path" --autoinstall="gmod_lua_bin_path" --chromium=1
```
