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

pip3 install --upgrade --user meson==1.8.2 || goto :error
meson setup _build -Dbackend_max_links=1 -Ddebug=false -Dwin32-backend=true -Dmedia-gstreamer=disabled -Dvulkan=disabled -Dsysprof=disabled -Dglib:sysprof=disabled -Daccesskit=enabled -Dlibxml2:werror=false -Daccesskit-c:triplet=%RUST_HOST% %~1 || goto :error
ninja -C _build || goto :error

goto :EOF
:error
exit /b 1
