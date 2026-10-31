@echo off
setlocal
set "LUNA_NEO_THEME=melody"
set "LUNA_WEB_UI=builtin"
"%~dp0..\install-neo-ui\luna-neo-ui.exe" %*
endlocal
