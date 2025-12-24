# tools/msys2/shell.ps1
param(
    [string]$msystem = "ucrt64",

    [Parameter(Position = 0)]
    [string[]]$cmd,

    [switch]$list
)

& $PSScriptRoot\run.ps1 $msystem @PSBoundParameters
