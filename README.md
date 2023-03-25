### EGSM
A module that enhances gmod with a thing that was asked for decades

Works only on the main and x86 branch

[Documentation](https://github.com/devonium/EGSM/wiki)

[Discord](https://discord.gg/X2Ay3cgW8T)
 
### How to install
* Put [the dll](https://github.com/devonium/EGSM/releases) in garrysmod/lua/bin
* APPEND `require((BRANCH == "x86-64" or BRANCH == "chromium" ) and "egsm_chromium" or "egsm")` to garrysmod/lua/menu/menu.lua

https://youtu.be/SlWGKU-mYRw

### How to compile
[garrysmod_common](https://github.com/danielga/garrysmod_common)
```
git clone --recursive https://github.com/danielga/garrysmod_common
git clone --recursive https://github.com/danielga/garrysmod_common -b "x86-64-support-sourcesdk"
```

```
premake5 vs2017 --gmcommon="garrysmod_common_path" --autoinstall="gmod_lua_bin_path"
premake5 vs2017 --gmcommon="garrysmod_common_x86_64_path" --autoinstall="gmod_lua_bin_path --chromium=1"
```
