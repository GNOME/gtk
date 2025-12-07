@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

@set RUST_HOST=x86_64-pc-windows-msvc

:: Needed for wget
@set MSYS2_BINDIR=c:\msys64\usr\bin

:: Download rust-init.exe if necessary
if not exist %HOMEPATH%\.cargo\bin\rustup.exe %MSYS2_BINDIR%\wget https://static.rust-lang.org/rustup/dist/%RUST_HOST%/rustup-init.exe

:: Check for updates in Rust, or install Rust using rustup-init.exe
if exist %HOMEPATH%\.cargo\bin\rustup.exe %HOMEPATH%\.cargo\bin\rustup update
if not exist %HOMEPATH%\.cargo\bin\rustup.exe rustup-init -y --default-toolchain=stable-%RUST_HOST% --default-host=%RUST_HOST%

@set PATH=%PATH%;%HOMEPATH%\.cargo\bin

:: If the existing toolchain isn't MSVC, add it as target
rustup target add %RUST_HOST% || goto :error

if not exist "_ccache" mkdir _ccache
set "CCACHE_BASEDIR=%CD%"
set "CCACHE_DIR=%CCACHE_BASEDIR%\_ccache"
:: Use depend_mode to reduce the cache miss overhead
:: https://ccache.dev/manual/latest.html#_the_depend_mode
set "CCACHE_DEPEND=1"
ccache --zero-stats
ccache --show-stats

pip3 install --upgrade --user meson==1.10.0rc2 || goto :error
set "CCACHE_DISABLE=true"
meson setup _build -Dbackend_max_links=1 -Ddebug=false -Dwin32-backend=true -Dmedia-gstreamer=disabled -Dvulkan=disabled -Dsysprof=disabled -Dglib:sysprof=disabled -Daccesskit=enabled -Daccesskit-c:triplet=%RUST_HOST% %~1 || goto :error
set "CCACHE_DISABLE="
ninja -C _build || goto :error
ccache --show-stats

goto :EOF
:error
exit /b 1
